#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define QUEUE_SIZE 100
#define NUMBER_OF_THREADS 5
#define TRUE 1

typedef struct {
    void (*function)(void *args);
    void *args;
} Task;

// Function Prototypes
void pool_init(int num_threads);
int pool_submit(void (*somefunction)(void *p), void *p);
void pool_shutdown(void);
void execute(void (*somefunction)(void *p), void *p);
void* worker(void *args);

// Shared Global Variables (extern so client.c can see them)
extern int total_tasks;
extern int completed_tasks;
extern sem_t all_tasks_completed;

#endif
