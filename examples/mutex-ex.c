#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "syscall.h"
#include "uart.h"
#include "mutex.h"

_pthread_mutex_t mutex_print;

void mutex_task1(void)
{
	set_program_name("mutex1");

	char *str = "mutex task 1\n\r";

	while(1) {
		pthread_mutex_lock(&mutex_print);
		uart3_puts(str);
		pthread_mutex_unlock(&mutex_print);
		sleep(100);
	}
}

void mutex_task2(void)
{
	set_program_name("mutex2");

	char *str = "mutex task 2\n\r";

	while(1) {
		pthread_mutex_lock(&mutex_print);
		uart3_puts(str);
		pthread_mutex_unlock(&mutex_print);
		sleep(100);
	}
}

void run_mutex_example(void)
{
	pthread_mutex_init(&mutex_print, 0);

	if(!fork()) mutex_task1();
	if(!fork()) mutex_task2();
}
