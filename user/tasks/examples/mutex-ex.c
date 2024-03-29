#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "uart.h"

#define BUFFER_SIZE 10

/* Producer and consumer's data */
static int my_buffer[BUFFER_SIZE];
static int my_cnt = 0;

/* For resource protection */
static pthread_mutex_t mutex;
static pthread_cond_t cond_producer;
static pthread_cond_t cond_consumer;

void mutex_task1(void)
{
    setprogname("mutex-ex-1");

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_producer, 0);
    pthread_cond_init(&cond_consumer, 0);

    int item = 1;

    while (1) {
        /* Start the critical section */
        pthread_mutex_lock(&mutex);

        if (my_cnt == BUFFER_SIZE) {
            printf("[mutex task 1] buffer is full, wait for consumer\n\r");
            pthread_cond_wait(&cond_producer, &mutex);
        }

        /* Produce an item and add it to the buffer */
        my_buffer[my_cnt++] = item;
        printf("[mutex task 1] produced item %d\n\r", item);
        item++;

        /* Signal to consumers that an item is available */
        pthread_cond_signal(&cond_consumer);

        /* End the critical section */
        pthread_mutex_unlock(&mutex);

        /* Simulate some work */
        sleep(1);
    }
}

void mutex_task2(void)
{
    setprogname("mutex-ex-2");

    while (1) {
        /* Start the critical section */
        pthread_mutex_lock(&mutex);

        if (my_cnt == 0) {
            printf("[mutex task 2] buffer is empty, wait for producer\n\r");
            pthread_cond_wait(&cond_consumer, &mutex);
        }

        /* Consume an item from the buffer */
        int item = my_buffer[--my_cnt];
        printf("[mutex task 2] consumed item %d\n\r", item);

        /* Signal to producers that there is space in the buffer */
        pthread_cond_signal(&cond_producer);

        /* End the critical section */
        pthread_mutex_unlock(&mutex);

        /* Simulate some work */
        usleep(100000); /* 100ms */
    }
}

HOOK_USER_TASK(mutex_task1, 0, STACK_SIZE_MIN);
HOOK_USER_TASK(mutex_task2, 0, STACK_SIZE_MIN);
