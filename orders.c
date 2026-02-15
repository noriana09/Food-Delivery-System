#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include "header.h"

_Atomic(struct PendingOrdersStack*) top = NULL;
_Atomic int elimination_success_count = 0;
_Atomic(struct UnderPreparationOrdersQueue*) headQ;
_Atomic(struct UnderPreparationOrdersQueue*) tail;
struct District Districts[DIST];
_Atomic(struct PendingOrdersStack*) elimination_array[ELIMINATION_ARRAY_SIZE];

// Pending Orders Stack

//Place an order temporarily in an array (elimination array) so that a pop thread can "find" it.
//If another thread picks up the order from that spot, it is considered a successful elimination
//and the order doesn't need to go to the stack.
int eliminate_push(struct PendingOrdersStack* node){
    for(int i=0; i<ELIMINATION_ARRAY_SIZE; ++i){
        struct PendingOrdersStack* e= NULL;
        if(atomic_compare_exchange_strong(&elimination_array[i], &e, node)){
            for(int j=0; j< 1000; ++j){
                if(atomic_load(&elimination_array[i]) == NULL){
                    atomic_fetch_add(&elimination_success_count,1);
                    return 0;
                }
            }
            struct PendingOrdersStack* e2=node;
            if(atomic_compare_exchange_strong(&elimination_array[i], &e2, NULL)){
                return 1;
            }else{
                atomic_fetch_add(&elimination_success_count, 1);
                return 0;
            }
        }
    }
    return 1;
}

//It searches the elimination array for orders that have been placed by an eliminate_push.
//If it finds one, it takes it and returns it.
struct PendingOrdersStack* eliminate_pop(void){
    for(int i=0; i<ELIMINATION_ARRAY_SIZE; ++i){
        struct PendingOrdersStack* node=atomic_load(&elimination_array[i]);
        if (node != NULL) {
            struct PendingOrdersStack* e=node;
            if(atomic_compare_exchange_strong(&elimination_array[i], &e, NULL)){
                return node;
            }
        }
    }
    return NULL;
}

//It creates a new order node and places it at the top of the stack using an atomic CAS.
//tries to use the elimination array to "exchange" it with a pop.
void push(Order id){
    sleep(0.005);
    struct PendingOrdersStack* new_node=malloc(sizeof(struct PendingOrdersStack));
    new_node->order_id=id;

    for (int i= 0; i< MAX_ATTEMPTS; ++i) {
        struct PendingOrdersStack* old_top=atomic_load(&top);
        new_node->next=old_top;
        if(atomic_compare_exchange_weak(&top, &old_top, new_node)){
            atomic_thread_fence(memory_order_release);
            return;
        }else if(eliminate_push(new_node) == 0){
            atomic_thread_fence(memory_order_release);
            return;
        }
    }
    while(1){
        struct PendingOrdersStack* old_top=atomic_load(&top);
        new_node->next=old_top;
        if(atomic_compare_exchange_weak(&top, &old_top, new_node)){
            atomic_thread_fence(memory_order_release);
            return;
        }
    }
}

//It takes the top of the stack
//and also tries the elimination array
struct PendingOrdersStack* pop(void){
    sleep(0.005);
    for (int i=0; i<MAX_ATTEMPTS; ++i) {
        atomic_thread_fence(memory_order_acquire);
        struct PendingOrdersStack* old_top=atomic_load(&top);
        if(old_top == NULL) return NULL;

        struct PendingOrdersStack* next=old_top->next;
        if(atomic_compare_exchange_weak(&top, &old_top, next)) return old_top;
        else{
            struct PendingOrdersStack* eliminated=eliminate_pop();
            if(eliminated) return eliminated;
        }
    }
    while(1){
        atomic_thread_fence(memory_order_acquire);
        struct PendingOrdersStack* old_top = atomic_load(&top);
        if(old_top == NULL) return NULL;

        struct PendingOrdersStack* next = old_top->next;
        if(atomic_compare_exchange_weak(&top, &old_top, next)) return old_top;
    }
}

// Under Preparation Queue

void init_queue(void) {
    struct UnderPreparationOrdersQueue* dummy=malloc(sizeof(struct UnderPreparationOrdersQueue));
    dummy->order_id=0;
    atomic_init(&dummy->next, NULL);
    atomic_store(&headQ, dummy);
    atomic_store(&tail, dummy);
}

//It creates a new node with an order_id
//It places the new node at the end of the queue using an atomic CAS
//If it fails, it updates the tail and retries until it succeeds
void enqueue(Order order_id) {
    struct UnderPreparationOrdersQueue* new_node=malloc(sizeof(struct UnderPreparationOrdersQueue));
    new_node->order_id=order_id;
    atomic_init(&new_node->next, NULL);

    while(1){
        struct UnderPreparationOrdersQueue* tail_old=atomic_load(&tail);
        struct UnderPreparationOrdersQueue* next=atomic_load(&tail_old->next);
        if(next==NULL){
            if(atomic_compare_exchange_weak(&tail_old->next, &next, new_node)){
                atomic_thread_fence(memory_order_release);
                atomic_compare_exchange_weak(&tail, &tail_old, new_node);
                return;
            }
        }else{
            atomic_compare_exchange_weak(&tail, &tail_old, next);
        }
    }
}

//It reads the first order from the queue.
//It copies the "order_id" to the variable "out_order"
bool dequeue(Order* out_order){
    while(1){
        atomic_thread_fence(memory_order_acquire);
        struct UnderPreparationOrdersQueue* head_old=atomic_load(&headQ);
        struct UnderPreparationOrdersQueue* tail_old=atomic_load(&tail);
        struct UnderPreparationOrdersQueue* next=atomic_load(&head_old->next);

        if(next == NULL) return false;

        if(head_old==tail_old){
            atomic_compare_exchange_weak(&tail,&tail_old,next);
        }else{
            if(atomic_compare_exchange_weak(&headQ, &head_old, next)){
                *out_order=next->order_id;
                return true;
            }
        }
    }
}

//Completed Orders List

void initDistricts(void){
    for(int i = 0; i < DIST; i++){
        Districts[i].completedOrders.order_id = 0;
        Districts[i].completedOrders.next = NULL;
        Districts[i].completedOrders.marked = 0;
        pthread_mutex_init(&Districts[i].completedOrders.lock, NULL);
        Districts[i].checksum = 0;
    }
}

//It checks that the nodes "pred" and "curr" are still valid
//they haven’t been deleted and are still connected
int validate(struct CompletedOrdersList* pred, struct CompletedOrdersList* curr){
    int p_active=(pred!=NULL)&&(!pred->marked);
    int c_active=(curr==NULL)||(!curr->marked);
    int linked=(pred!=NULL)&&(pred->next==curr);
    return p_active && c_active && linked;
}

//inserts an order with order_id into the correct position in the sorted list of region pos
int insert(int pos, Order newOrder){
    struct CompletedOrdersList *pred, *curr;
    struct CompletedOrdersList* head=&Districts[pos].completedOrders;
    while (1) {
        pred=head;
        curr=pred->next;
        while (curr != NULL && curr->order_id < newOrder) {
            pred=curr;
            curr=curr->next;
        }
        if(curr!=NULL && curr<pred){
            pthread_mutex_lock(&curr->lock);
            pthread_mutex_lock(&pred->lock);
        }else{
            pthread_mutex_lock(&pred->lock);
            if(curr!=NULL) pthread_mutex_lock(&curr->lock);
        }

        if(validate(pred, curr)){
            if(curr!=NULL && curr->order_id==newOrder){
                if(curr!=NULL) pthread_mutex_unlock(&curr->lock);
                pthread_mutex_unlock(&pred->lock);
                return 0;
            }
            struct CompletedOrdersList* newNode=malloc(sizeof(struct CompletedOrdersList));
            if(!newNode){
                if(curr != NULL) pthread_mutex_unlock(&curr->lock);
                pthread_mutex_unlock(&pred->lock);
                return -1;
            }

            newNode->order_id=newOrder;
            newNode->marked=0;
            newNode->next=curr;
            pthread_mutex_init(&newNode->lock, NULL);
            atomic_thread_fence(memory_order_release);
            pred->next=newNode;
            if(curr!=NULL)pthread_mutex_unlock(&curr->lock);
            pthread_mutex_unlock(&pred->lock);
            return 1;
        }

        if (curr!=NULL) pthread_mutex_unlock(&curr->lock);
        pthread_mutex_unlock(&pred->lock);
    }
}
