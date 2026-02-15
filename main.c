#include "header.h"
#include <pthread.h>
#include <unistd.h>


const char* PassStr(int pass) {
    return (pass ? "PASS" : "FAIL");
}

void PrintPendingOrdersEmpty(int pass, size_t n) {
    const char* passStr = PassStr(pass);
    printf("%s PendingOrders Empty %lu\n", passStr, n);
}

void PrintUnderPreparationOrdersEmpty(int pass, size_t n) {
    const char* passStr = PassStr(pass);
    printf("%s UnderPreparationOrders Empty %lu\n", passStr, n);
}

void PrintCompletedOrdersSum(int pass, size_t tid, size_t sum) {
    const char* passStr = PassStr(pass);
    printf("%s District[%lu].completedOrders Sum %lu\n", passStr, tid, sum);
}

void PrintCompletedOrdersSize(bool pass, size_t tid, size_t n) {
    const char* passStr = PassStr(pass);
    printf("%s District[%lu].completedOrders Size %lu\n", passStr, tid, n);
}

void PrintCompletedOrdersValid(int pass, size_t tid, size_t checksum) {
    const char* passStr = PassStr(pass);
    printf("%s District[%lu].checksum Valid %lu\n", passStr, tid, checksum);
}


// Each customer (thread) creates DIST orders,
// pushes them into the Pending Orders stack,
// then repeatedly searches for their own orders in their personal list.
// Once all their orders are found, they sum their IDs to compute the checksum.
void* customers(void* arg) {
    int tid = *(int*)arg;
    free(arg);
    int district_id = tid % DIST;
    Order orders[DIST];
    for (int i = 0; i < DIST; ++i) {
        orders[i] = (district_id * DIST) + i;
        push(orders[i]);
    }
    int seen = 0;
    int found[DIST] = {0};
    while (seen < DIST) {
        pthread_mutex_lock(&Districts[district_id].completedOrders.lock);
        struct CompletedOrdersList* curr = Districts[district_id].completedOrders.next;
        while (curr != NULL) {
            for (int i = 0; i < DIST; ++i) {
                if (!found[i]) {  
                }
                if (!found[i] && curr->order_id == orders[i]) {
                    found[i] = 1;
                    seen++;
                    atomic_fetch_add(&Districts[district_id].checksum, orders[i]);
                }
            }
            curr = curr->next;
        }
        pthread_mutex_unlock(&Districts[district_id].completedOrders.lock);
        if (seen < DIST) sleep(1);
    }
    return NULL;
}

// Each agent pops 2*DIST orders from the stack
// and enqueues them into the Preparation Queue.
void* agents(void* arg) {
    free(arg);
    for (int i=0; i <2*DIST; ++i) {
        struct PendingOrdersStack* order = pop();
        if (order != NULL) {
            enqueue(order->order_id);
            free(order);
        } else {
            sleep(0.005);
            --i;
        }
    }
    return NULL;
}

// The cooks dequeue 2*DIST orders from the queue,
// determine which customer each order belongs to using tid = order_id / DIST,
// and insert them into the list corresponding to that customer.
void* cookers(void* arg){
    free(arg);
    for(int i=0; i < 2*DIST; ++i){
        Order order_id;
        while(!dequeue(&order_id)){
            sleep(0.005);
        }
        int tid=order_id/DIST;
        insert(tid, order_id);
    }
    return NULL;
}

// The main thread creates all the customer, agent, and cook threads and waits for them to finish.
// In the end, it verifies that the stack is empty,
// each region list contains DIST orders,
// and the sum of the order IDs (checksum) is correct.
void main_thread(void){
    pthread_t customers_threads[DIST];
    pthread_t agent_threads[AGNT];
    pthread_t cook_threads[COOK];

    initDistricts();
    init_queue();

    for(int i=0; i<DIST; ++i){
        int* tid=malloc(sizeof(int));
        *tid=i;
        pthread_create(&customers_threads[i], NULL, customers, tid);
    }
    for(int i=0; i<AGNT; ++i){
        int* tid=malloc(sizeof(int)); 
        *tid=i;
        pthread_create(&agent_threads[i], NULL, agents, tid);
    }
    for(int i=0; i<COOK; ++i){
        int* tid=malloc(sizeof(int)); 
        *tid=i;
        pthread_create(&cook_threads[i], NULL, cookers, tid);
    }
    for(int i=0; i<DIST; ++i)
        pthread_join(customers_threads[i], NULL);
    for(int i=0; i<AGNT; ++i)
        pthread_join(agent_threads[i], NULL);
    for(int i=0; i<COOK; ++i)
        pthread_join(cook_threads[i], NULL);

    //Pending Orders Stack check
    size_t count=0;
    struct PendingOrdersStack* tmp=atomic_load(&top);
    while(tmp != NULL){
        count++;
        tmp=tmp->next;
    }
    PrintPendingOrdersEmpty(count==0,count);

    //UnderPreparation Queue check
    size_t u_count=0;
    struct UnderPreparationOrdersQueue* curr=atomic_load(&headQ)->next;
    while(curr != NULL){
        u_count++;
        curr=atomic_load(&curr->next);
    }
    PrintUnderPreparationOrdersEmpty(u_count == 0, u_count);

    //District lists check
    for(size_t i = 0; i < DIST; ++i){
        size_t count=0;
        size_t sum=0;
        struct CompletedOrdersList* curr=Districts[i].completedOrders.next;
        while(curr != NULL){
            pthread_mutex_lock(&curr->lock);
            if(!curr->marked){
                count++;
                sum += curr->order_id;
            }
            pthread_mutex_unlock(&curr->lock);
            curr=curr->next;
        }

        size_t expected_sum=(i * DIST * DIST) + ((DIST - 1) * DIST) / 2;

        PrintCompletedOrdersSum(sum==expected_sum, i, sum);
        PrintCompletedOrdersValid(Districts[i].checksum ==sum, i, Districts[i].checksum);
        PrintCompletedOrdersSize(count==DIST, i, count);
    }

    // Destroy mutexes
    for(size_t i = 0; i < DIST; ++i){
        pthread_mutex_destroy(&Districts[i].completedOrders.lock);
        struct CompletedOrdersList* curr=Districts[i].completedOrders.next;
        while(curr != NULL){
            pthread_mutex_destroy(&curr->lock);
            curr=curr->next;
        }
    }
}

int main(void){
    main_thread();
    return 0;
}
