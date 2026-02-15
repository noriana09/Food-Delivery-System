# Makefile for multithreaded C program
N_THREADS ?=4000

CC = gcc
CFLAGS = -g3 -O0 -std=c11 -Wall -Wextra -Wpedantic -pthread  -DN_THREADS=$(N_THREADS)
LDLIBS = -pthread 
SRC = main.c orders.c
INCLUDE = header.h
TARGET = main

.PHONY: all clean valgrind asan

all: $(TARGET)

$(TARGET): $(SRC) $(INCLUDE) Makefile
	@echo "Building with N_THREADS = $(N_THREADS)"
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

valgrind: all
	valgrind --tool=memcheck --track-origins=yes --leak-check=full ./$(TARGET)

clean:
	rm -f $(TARGET)
