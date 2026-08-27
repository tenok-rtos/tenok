Tenok API List
==============

## User-space API

### Utilities:

* thread_info()

* minfo()

### Scheduler:

* sched_start()

* sched_yield()

* sched_get_priority_max()

* sched_get_priority_min()

* sched_rr_get_interval()

* sleep()

* usleep()

* delay_ticks()

### Task:

* HOOK_USER_TASK()

* task_create()

* setprogname()

* prctl()

* getpid()

* exit()

### Thread:

* pthread_attr_init()

* pthread_attr_destroy()

* pthread_attr_getdetachstate()

* pthread_attr_setinheritsched()

* pthread_attr_getinheritsched()

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

* pthread_setcancelstate()

* pthread_setcanceltype()

* pthread_testcancel()

* pthread_cleanup_push()

* pthread_cleanup_pop()

* pthread_detach()

* pthread_yield()

* pthread_exit()

* pthread_equal()

* pthread_kill()

* pthread_once()

* pthread_atfork()

* pthread_key_create()

* pthread_key_delete()

* pthread_setspecific()

* pthread_getspecific()

* pthread_self()

* gettid()

### Mutex and Conditional Variable:

* pthread_mutex_init()

* pthread_mutex_destroy()

* pthread_mutex_lock()

* pthread_mutex_trylock()

* pthread_mutex_unlock()

* pthread_mutexattr_init()

* pthread_mutexattr_destroy()

* pthread_mutexattr_setprotocol()

* pthread_mutexattr_getprotocol()

* pthread_mutexattr_settype()

* pthread_mutexattr_gettype()

* pthread_cond_init()

* pthread_cond_destroy()

* pthread_cond_signal()

* pthread_cond_broadcast()

### Read-Write Lock:

* pthread_rwlockattr_init()

* pthread_rwlockattr_destroy()

* pthread_rwlock_init()

* pthread_rwlock_destroy()

* pthread_rwlock_rdlock()

* pthread_rwlock_tryrdlock()

* pthread_rwlock_wrlock()

* pthread_rwlock_trywrlock()

* pthread_rwlock_unlock()

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

* pipe()

* fcntl()

* ioctl()

* poll()

* lseek()

* ftruncate()

* dup()

* dup2()

* isatty()

* access()

* mkfifo()

* mknod()

* mount()

* statfs()

* opendir()

* readdir()

* closedir()

* rewinddir()

* getcwd()

* chdir()

* fchdir()

* chroot()

* ttyname_r()

### Files and Directories:

* stat()

* lstat()

* fstat()

* chmod()

* fchmod()

* umask()

* chown()

* lchown()

* fchown()

* utime()

* utimes()

* futimens()

* mkdir()

* rmdir()

* unlink()

* remove()

* rename()

* link()

* symlink()

* readlink()

* mkstemp()

### Streams:

* fopen()

* fdopen()

* freopen()

* fclose()

* fread()

* fwrite()

* fseek()

* fseeko()

* ftell()

* ftello()

* rewind()

* fileno()

* feof()

* ferror()

* clearerr()

* fflush()

* setbuf()

* setvbuf()

* fgetc()

* getchar()

* fgets()

* fputc()

* putchar()

* fputs()

* puts()

* printf()

* fprintf()

* dprintf()

* sprintf()

* snprintf()

* vprintf()

* vfprintf()

* vdprintf()

* vsprintf()

* vsnprintf()

* sscanf()

* vsscanf()

* fscanf()

* popen()

* pclose()

### Process:

Tenok has neither `fork()` nor `exec()`: a task comes from the image the system
booted and is reached by calling it. The calls are named so that a program
written for POSIX compiles, and each says so when it is made.

* getpid()

* getppid()

* fork()

* vfork()

* execve()

* execvp()

* wait()

* waitpid()

* setsid()

### User and Group:

Tenok runs as the one user it has.

* getuid()

* geteuid()

* getgid()

* getegid()

* getgroups()

* setuid()

* seteuid()

* setgid()

* setegid()

* getpwuid()

* getpwnam()

* getgrgid()

* getgrnam()

### System:

* uname()

* sysconf()

* getrlimit()

* setrlimit()

* gettimeofday()

* settimeofday()

### Network:

Tenok carries no network. The calls are named so that a program written for
POSIX compiles, and each says so when it is made.

* socket()

* bind()

* listen()

* sendto()

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

* timer_settime()

* timer_gettime()

* timer_delete()

* clock_getres()

* clock_gettime()

* clock_settime()

* time()

* strptime()

* nanosleep()

### Memory Allocation:

* mpool_init()

* mpool_alloc()

* malloc()

* posix_memalign()

* calloc()

* realloc()

* free()

* alloca()

---

## Kernel-space API

### Scheduler

* schedule()

### Kernel Thread

* kthread_create()

### Logging:

* printk()

* panic()

### Interrupt:

* preempt_disable()

* preempt_enable()

### Mutex:

* mutex_init()

* mutex_is_locked()

* mutex_trylock()

* mutex_lock()

* mutex_unlock()

### Semaphore:

* sema_init()

* up()

* down()

* down_trylock()

### Tasklet (SoftIRQ):

* tasklet_init()

* tasklet_schedule()

### Wait Queue:

* DECLARE_WAIT_QUEUE_HEAD()

* init_waitqueue_head()

* wait_event()

* prepare_to_wait()

* finish_wait()

* wake_up()

* wake_up_all()

### kfifo:

* kfifo_init()

* kfifo_alloc()

* kfifo_free()

* kfifo_in()

* kfifo_out()

* kfifo_out_peek()

* kfifo_dma_in_prepare()

* kfifo_dma_in_finish()

* kfifo_dma_out_prepare()

* kfifo_dma_out_finish()

* kfifo_put()

* kfifo_get()

* kfifo_peek()

* kfifo_peek_len()

* kfifo_skip()

* kfifo_avail()

* kfifo_len()

* kfifo_esize()

* kfifo_size()

* kfifo_header_size()

* kfifo_is_empty()

* kfifo_is_full()

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

---

## Common API

### List:

* LIST_HEAD()

* LIST_HEAD_INIT()

* INIT_LIST_HEAD()

* list_add()

* list_add_tail()

* list_del()

* list_del_init()

* list_empty()

* list_is_last()

* list_move()

* list_move_tail() 

* list_entry()

* list_first_entry()

* list_next_entry()

* list_prev_entry()

* list_entry_is_head()

* list_for_each()

* list_for_each_entry()

* list_for_each_safe()
