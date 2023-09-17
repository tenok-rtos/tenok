#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <kernel/task.h>

#include <tenok/tenok.h>
#include <tenok/fcntl.h>
#include <tenok/unistd.h>
#include <tenok/pthread.h>

#include "uart.h"

#define BUFFER_SIZE 10

/* costomized printf */
#define printf(...) \
    sprintf(pbuf, __VA_ARGS__); \
    print(serial_fd, pbuf);

char pbuf[200] = {0};

/* producer and consumer's data */
int my_buffer[BUFFER_SIZE];
int my_cnt = 0;

/* data protection and task signaling */
pthread_mutex_t mutex;
pthread_cond_t cond_producer;
pthread_cond_t cond_consumer;

static void print(int fd, char *str)
{
    int len = strlen(str);
    write(fd, str, len);
}

void mutex_task1(void)
{
    setprogname("mutex1");

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_producer, 0);
    pthread_cond_init(&cond_consumer, 0);

    int serial_fd = open("/dev/serial0", 0);

    int item = 1;

    while(1) {
        /* start of the critical section */
        pthread_mutex_lock(&mutex);

        if (my_cnt == BUFFER_SIZE) {
            printf("[task 1] buffer is full. producer is waiting...\n\r");
            pthread_cond_wait(&cond_producer, &mutex);
        }

        /* produce an item and add it to the buffer */
        my_buffer[my_cnt++] = item;
        printf("[task 1] produced item %d\n\r", item);
        item++;

        /* signal to consumers that an item is available */
        pthread_cond_signal(&cond_consumer);

        /* end of the critical section */
        pthread_mutex_unlock(&mutex);

        /* simulate some work */
        sleep(1);
    }
}

void mutex_task2(void)
{
    setprogname("mutex2");

    int serial_fd = open("/dev/serial0", 0);

    while(1) {
        /* start of the critical section */
        pthread_mutex_lock(&mutex);

        if (my_cnt == 0) {
            printf("[task 2] buffer is empty. consumer is waiting...\n\r");
            pthread_cond_wait(&cond_consumer, &mutex);
        }

        /* consume an item from the buffer */
        int item = my_buffer[--my_cnt];
        printf("[task 2] consumed item %d\n\r", item);

        /* signal to producers that there is space in the buffer */
        pthread_cond_signal(&cond_producer);

        /* end of the critical section */
        pthread_mutex_unlock(&mutex);

        /* simulate some work */
        usleep(100000); //100ms
    }
}

HOOK_USER_TASK(mutex_task1);
HOOK_USER_TASK(mutex_task2);
