Tenok API List
==============

## User-space API

### Task:

* HOOK_USER_TASK()

* task_create()

* getpid()

* exit()

### Scheduler:

* sleep()

* usleep()

* sched_yield()

* sched_get_priority_max()

* sched_get_priority_min()

* sched_rr_get_interval()

### Thread:

* pthread_attr_init()

* pthread_attr_destroy()

* pthread_attr_getdetachstate()

* pthread_attr_getschedparam()

* pthread_attr_getschedpolicy()

* pthread_attr_getstackaddr()

* pthread_attr_getstacksize()

* pthread_attr_setdetachstate()

* pthread_attr_setschedparam()

* pthread_attr_setschedpolicy()

* pthread_attr_setstackaddr()

* pthread_attr_setstacksize()

* pthread_setschedparam()

* pthread_getschedparam()

* pthread_create()

* pthread_join()

* pthread_cancel()

* pthread_detach()

* pthread_yield()

* pthread_exit()

* pthread_equal()

* pthread_kill()

* pthread_once()

* pthread_self()

### Mutex and Condition Variable:

* pthread_mutex_init()

* pthread_mutex_destroy()

* pthread_mutex_lock()

* pthread_mutex_trylock()

* pthread_mutex_unlock()

* pthread_mutexattr_init()

* pthread_mutexattr_destroy()

* pthread_mutexattr_setprotocol()

* pthread_mutexattr_getprotocol()

* pthread_cond_init()

* pthread_cond_destroy()

* pthread_cond_signal()

* pthread_cond_broadcast()

* pthread_cond_wait()

* pthread_condattr_init()

* pthread_condattr_destroy()

### Semaphore:

* sem_init()

* sem_destroy()

* sem_getvalue()

* sem_post()

* sem_wait()

* sem_trywait()

### Message Queue:

* mq_open()

* mq_close()

* mq_send()

* mq_receive()

* mq_setattr()

* mq_getattr()

### File Control and I/O:

* open()

* close()

* read()

* write()

* ioctl():

* poll()

* lseek()

* dup()

* dup2()

* mkfifo()

* mknod()

* mount()

* opendir()

* readdir()

* fclose()

* fileno()

* fopen()

* fread()

* fseek()

* fstat() 

* fwrite() 

### Signals:

* sigaction()

* sigaddset()

* sigdelset()

* sigemptyset()

* sigfillset()

* sigismember()

* sigwait()

* raise()

* pause()

* kill()

### Timer and Clock:

* timer_create()

* timer_delete()

* timer_gettime()

* timer_settime()

* clock_getres()

* clock_gettime()

* clock_settime()

### Memory Allocation:

* mpool_init()

* mpool_alloc()

* malloc()

* calloc()

* free()

---

## Kernel-space API

### Mutex:

* mutex_init()

* mutex_is_locked()

* mutex_lock()

* mutex_unlock()

### Semaphore:

* sema_init()

* up()

* down()

* down_trylock()

### Tasklet:

* tasklet_init()

* tasklet_schedule()

### Waitqueue:

* prepare_to_wait()

* finish_wait()

* wake_up()

* wake_up_all()

### kfifo:

* kfifo_alloc()

* kfifo_avail()

* kfifo_esize()

* kfifo_free()

* kfifo_get()

* kfifo_in()

* kfifo_init()

* kfifo_is_empty()

* kfifo_is_full()

* kfifo_len()

* kfifo_out()

* kfifo_out_peek()

* kfifo_peek()

* kfifo_peek_len()

* kfifo_put()

* kfifo_size()

* kfifo_skip()

### List:

* LIST_HEAD()

* LIST_HEAD_INIT()

* INIT_LIST_HEAD()

* list_add()

* list_del()

* list_del_init()

* list_empty()

* list_is_last()

* list_move() 

* list_entry()

* list_entry_is_head()

* list_first_entry()

* list_for_each()

* list_for_each_entry()

* list_for_each_safe()

* list_next_entry()

* list_prev_entry()

### Memory Allocation:

* kmalloc()

* kfree()

* page_order_to_size()

* size_to_page_order()

* alloc_pages()

* free_pages()

* kmem_cache_alloc()

* kmem_cache_create()

* kmem_cache_free()

### Interrupt:

* preempt_disable()

* preempt_enable()

* request_irq()

### Logging:

* printk()
