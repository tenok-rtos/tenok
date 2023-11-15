#include <stdio.h>

#include <kernel/mutex.h>
#include <kernel/semaphore.h>
#include <kernel/thread.h>

#define PRINT_SIZE(size_macro, type) \
    printf("%s = %d\n", #size_macro, sizeof(type))

int main(void)
{
    PRINT_SIZE(__SIZEOF_PTHREAD_MUTEXATTR_T, struct mutex_attr);
    PRINT_SIZE(__SIZEOF_PTHREAD_MUTEX_T, struct mutex);
    PRINT_SIZE(__SIZEOF_PTHREAD_ATTR_T, struct thread_attr);
    PRINT_SIZE(__SIZEOF_PTHREAD_COND_T, struct cond);
    PRINT_SIZE(__SIZEOF_PTHREAD_ONCE_T, struct thread_once);
    PRINT_SIZE(__SIZEOF_SEM_T, struct semaphore);

    return 0;
}
