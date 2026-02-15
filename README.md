# Food Delivery Simulation System

## Description

This project is a multithreaded simulation of a food delivery system.

It is implemented in C using the pthreads library and atomic operations.

The system simulates how food orders are:
1. Created
2. Recorded
3. Prepared
4. Verified

The goal of the project is to demonstrate concurrent programming using lock-free data structures.

---

## Thread Types

The system contains four types of threads:

### 1. District (Customer) Threads
- Generate food orders.
- Push orders into a shared stack.
- Wait until their orders appear in their district list.
- Calculate a checksum of their orders.

### 2. Agent Threads
- Pop orders from the stack.
- Enqueue them into a shared preparation queue.

### 3. Cook Threads
- Dequeue orders from the queue.
- Insert them into the correct district list.

### 4. Main Thread
- Creates all threads.
- Waits for them to finish.
- Performs correctness verification.

---

## Data Structures Used

The project uses concurrent shared data structures:

- Lock-Free Elimination Backoff Stack (Pending Orders)
- Lock-Free Queue (Under Preparation Orders)
- Lazy Synchronization Linked List (Completed Orders per district)

Each district also maintains a checksum to verify correctness.

---

## Program Flow

1. District threads generate DIST orders each.
2. Agents move orders from the stack to the queue.
3. Cooks move orders from the queue to district lists.
4. District threads detect their completed orders.
5. Main thread verifies correctness.

---

## Correctness Checks

After all threads finish, the program checks:

- The stack is empty.
- The queue is empty.
- Each district list contains DIST elements.
- The sum of order IDs is correct.
- The checksum matches the list sum.

The program prints `PASS` or `FAIL` for each check.

---

## Thread Configuration

The number of threads is defined at compile time using:

-DN_THREADS=<number>

Constraints:

- N_THREADS must be divisible by 4.
- DIST = N_THREADS / 2
- AGNT = N_THREADS / 4
- COOK = N_THREADS / 4

You can change N_THREADS inside the Makefile.

---

## How to Build and Run

### 1. Compile

make

./main



---

## Requirements

- GCC
- pthreads
- C11 support

Compile flags used:

- -std=c11
- -pthread
- -DN_THREADS=<number>

---

## What This Project Demonstrates

- Lock-free programming
- Atomic operations
- Memory ordering
- Elimination backoff technique
- Concurrent queue implementation
- Lazy synchronization
- Multithreaded system design

