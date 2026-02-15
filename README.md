# Food Delivery Simulation System (HY-486 Project)

## Overview

This project is a multithreaded simulation of a food delivery system, implemented in C using the `pthreads` library.

The system models how food orders are created, processed, prepared, and verified using concurrent data structures.

It was developed for the HY-486 course (Distributed Computing Principles).

---

## What the Program Does

The system simulates four types of threads:

- **District (Customer) Threads**  
  Create food orders and push them into a shared stack.

- **Agent Threads**  
  Remove orders from the stack and move them into a shared queue.

- **Cook Threads**  
  Remove orders from the queue and insert them into the correct district list.

- **Main Thread**  
  Creates all threads, waits for them to finish, and verifies correctness.

---

## Data Structures Used

The system uses three shared concurrent data structures:

- **Lock-Free Elimination Backoff Stack**  
  Stores pending orders.

- **Lock-Free Queue (Michael & Scott style)**  
  Stores orders under preparation.

- **Lazy Synchronization Linked List**  
  Stores completed orders per district.

Each district also maintains a checksum to verify correctness.

---

## Correctness Checks

After all threads finish, the program verifies that:

- The stack is empty.
- The queue is empty.
- Each district list contains the correct number of orders.
- The sum of orders is correct.
- The checksum matches the list sum.

The program prints `PASS` or `FAIL` messages for each check.

---

## Thread Configuration

The number of threads is controlled at compile time using:

