#ifndef HEADER_H
#define HEADER_H

#include <stddef.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef N_THREADS
#define N_THREADS 4
#endif

_Static_assert(N_THREADS > 0, "N_THREADS must be positive");
_Static_assert((N_THREADS % 4) == 0, "N_THREADS must be multiple of 4");

#define DIST (N_THREADS/2)
#define AGNT (N_THREADS/4)
#define COOK (N_THREADS/4)

#define ELIMINATION_ARRAY_SIZE (N_THREADS/2)
#define MAX_ATTEMPTS (N_THREADS/4)

typedef size_t Order;

//Pending Orders Stack
struct PendingOrdersStack {
    Order order_id;
    struct PendingOrdersStack* next;
};

extern _Atomic(struct PendingOrdersStack*) top;
extern _Atomic int elimination_success_count;

//Place an order temporarily in an array (elimination array) so that a pop thread can "find" it.
//If another thread picks up the order from that spot, it is considered a successful elimination
//and the order doesn't need to go to the stack.
int eliminate_push(struct PendingOrdersStack* node);

//It searches the elimination array for orders that have been placed by an eliminate_push.
//If it finds one, it takes it and returns it.
struct PendingOrdersStack* eliminate_pop(void);

//It creates a new order node and places it at the top of the stack using an atomic CAS.
//tries to use the elimination array to "exchange" it with a pop.
void push(Order id);

//It takes the top of the stack
//and also tries the elimination array
struct PendingOrdersStack* pop(void);


//Under Preparation Orders Queue
struct UnderPreparationOrdersQueue {
    Order order_id;
    _Atomic(struct UnderPreparationOrdersQueue*) next;
};

extern _Atomic(struct UnderPreparationOrdersQueue*) headQ;
extern _Atomic(struct UnderPreparationOrdersQueue*) tail;

//It creates a new node with an order_id
//It places the new node at the end of the queue using an atomic CAS
//If it fails, it updates the tail and retries until it succeeds
void enqueue(Order order_id);

//It reads the first order from the queue.
//It copies the "order_id" to the variable "out_order"
bool dequeue(Order* out_order);
void init_queue(void);


//Completed Orders List
struct CompletedOrdersList {
    Order order_id;
    struct CompletedOrdersList* next;
    int marked;
    pthread_mutex_t lock;
};

struct District {
    struct CompletedOrdersList completedOrders;
    _Atomic size_t checksum;
};

extern struct District Districts[DIST];

void initDistricts(void);

//It checks that the nodes "pred" and "curr" are still valid
//they haven’t been deleted and are still connected
int validate(struct CompletedOrdersList* pred, struct CompletedOrdersList* curr);

//inserts an order with order_id into the correct position in the sorted list of region pos
int insert(int pos, Order newOrder);

#endif
