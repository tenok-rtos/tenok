#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>

#include <arch/port.h>
#include <common/list.h>
#include <kernel/errno.h>
#include <kernel/mutex.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>

#include "kconfig.h"

/* A program declares its objects with the opaque types, and the calls below
 * write to the structures through them. The two sizes are written down in two
 * places and nothing but this says they still agree; when they stop agreeing,
 * a call writes past the end of what the program set aside
 */
_Static_assert(sizeof(pthread_mutexattr_t) == sizeof(struct mutex_attr),
               "pthread_mutexattr_t no longer holds a struct mutex_attr");
_Static_assert(sizeof(pthread_mutex_t) == sizeof(struct mutex),
               "pthread_mutex_t no longer holds a struct mutex");
_Static_assert(sizeof(pthread_attr_t) == sizeof(struct thread_attr),
               "pthread_attr_t no longer holds a struct thread_attr");
_Static_assert(sizeof(pthread_cond_t) == sizeof(struct cond),
               "pthread_cond_t no longer holds a struct cond");
_Static_assert(sizeof(pthread_once_t) == sizeof(struct thread_once),
               "pthread_once_t no longer holds a struct thread_once");
_Static_assert(sizeof(pthread_rwlock_t) == sizeof(struct rwlock),
               "pthread_rwlock_t no longer holds a struct rwlock");

int pthread_attr_init(pthread_attr_t *attr)
{
    if (!attr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->schedparam.sched_priority = 0;
    _attr->stacksize = STACK_SIZE_MIN;
    _attr->stackaddr = NULL;
    _attr->schedpolicy = SCHED_RR;
    _attr->inheritsched = PTHREAD_EXPLICIT_SCHED;
    _attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    if (!attr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    memset(_attr, 0, sizeof(struct thread_attr));

    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *attr,
                               const struct sched_param *param)
{
    if (!attr || !param)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->schedparam = *param;

    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *attr,
                               struct sched_param *param)
{
    if (!attr || !param)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *param = _attr->schedparam;

    return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
    if (!attr)
        return EINVAL;

    if (policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_OTHER &&
        policy != SCHED_SPORADIC)
        return EINVAL;

    /* Round robin is the only way Tenok schedules, and the object is left as
     * it was. Saying so here is what lets a caller read the policy back and
     * see what it really got, rather than find out when the thread it makes
     * with the object fails to be created
     */
    if (policy != SCHED_RR)
        return ENOTSUP;

    ((struct thread_attr *) attr)->schedpolicy = policy;

    return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
    if (!attr || !policy)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *policy = _attr->schedpolicy;

    return 0;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol)
{
    if (!attr)
        return EINVAL;

    struct mutex_attr *mtx_attr = (struct mutex_attr *) attr;
    mtx_attr->protocol = protocol;

    return 0;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr,
                                  int *protocol)
{
    if (!attr || !protocol)
        return EINVAL;

    struct mutex_attr *mtx_attr = (struct mutex_attr *) attr;
    *protocol = mtx_attr->protocol;

    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    if (!attr)
        return EINVAL;

    if (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_RECURSIVE)
        return EINVAL;

    ((struct mutex_attr *) attr)->type = type;

    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
    if (!attr || !type)
        return EINVAL;

    *type = ((struct mutex_attr *) attr)->type;

    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    if (!attr)
        return EINVAL;

    /* Say so here rather than let the thread run off the end of a stack that
     * was never going to be enough
     */
    if (stacksize < STACK_SIZE_MIN)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->stacksize = stacksize;

    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    if (!attr || !stacksize)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *stacksize = _attr->stacksize;

    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
    if (!attr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;

    if (detachstate != PTHREAD_CREATE_DETACHED &&
        detachstate != PTHREAD_CREATE_JOINABLE) {
        return EINVAL;
    }

    _attr->detachstate = detachstate;

    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
    if (!attr || !detachstate)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *detachstate = _attr->detachstate;

    return 0;
}

int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched)
{
    if (!attr)
        return EINVAL;

    if (inheritsched != PTHREAD_INHERIT_SCHED &&
        inheritsched != PTHREAD_EXPLICIT_SCHED)
        return EINVAL;

    ((struct thread_attr *) attr)->inheritsched = inheritsched;

    return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched)
{
    if (!attr || !inheritsched)
        return EINVAL;

    *inheritsched = ((struct thread_attr *) attr)->inheritsched;

    return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
    if (!attr)
        return EINVAL;

    if (!stackaddr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->stackaddr = stackaddr;

    return 0;
}

int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
    if (!attr || !stackaddr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *stackaddr = _attr->stackaddr;

    return 0;
}

/* A thread call answers with the number instead of leaving it in errno */
static inline int set_error(int retval)
{
    return (retval < 0) ? -retval : retval;
}

static NACKED int __pthread_create(pthread_t *thread,
                                   const pthread_attr_t *attr,
                                   void *(*start_routine)(void *),
                                   void *arg)
{
    SYSCALL(PTHREAD_CREATE);
}

int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*start_routine)(void *),
                   void *arg)
{
    return set_error(__pthread_create(thread, attr, start_routine, arg));
}

NACKED pthread_t pthread_self(void)
{
    SYSCALL(PTHREAD_SELF);
}

static NACKED int __pthread_join(pthread_t thread, void **retval)
{
    SYSCALL(PTHREAD_JOIN);
}

int pthread_join(pthread_t thread, void **retval)
{
    int ret = set_error(__pthread_join(thread, retval));

    pthread_testcancel();

    return ret;
}

static NACKED int __pthread_detach(pthread_t thread)
{
    SYSCALL(PTHREAD_DETACH);
}

int pthread_detach(pthread_t thread)
{
    return set_error(__pthread_detach(thread));
}

static NACKED int __pthread_cancel(pthread_t thread)
{
    SYSCALL(PTHREAD_CANCEL);
}

int pthread_cancel(pthread_t thread)
{
    return set_error(__pthread_cancel(thread));
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1 == t2;
}

static NACKED int __pthread_setschedparam(pthread_t thread,
                                          int policy,
                                          const struct sched_param *param)
{
    SYSCALL(PTHREAD_SETSCHEDPARAM);
}

int pthread_setschedparam(pthread_t thread,
                          int policy,
                          const struct sched_param *param)
{
    return set_error(__pthread_setschedparam(thread, policy, param));
}

static NACKED int __pthread_getschedparam(pthread_t thread,
                                          int *policy,
                                          struct sched_param *param)
{
    SYSCALL(PTHREAD_GETSCHEDPARAM);
}

int pthread_getschedparam(pthread_t thread,
                          int *policy,
                          struct sched_param *param)
{
    return set_error(__pthread_getschedparam(thread, policy, param));
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    if (!attr)
        return EINVAL;

    struct mutex_attr *_attr = (struct mutex_attr *) attr;
    _attr->protocol = PTHREAD_PRIO_NONE;
    _attr->type = PTHREAD_MUTEX_DEFAULT;

    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    if (!attr)
        return EINVAL;

    memset(attr, 0, sizeof(pthread_mutexattr_t));
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    if (!mutex)
        return EINVAL;

    struct mutex *_mutex = (struct mutex *) mutex;
    __mutex_init(_mutex);

    if (attr) {
        struct mutex_attr *_attr = (struct mutex_attr *) attr;
        _mutex->protocol = _attr->protocol;
        _mutex->type = _attr->type;
    }

    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (!mutex)
        return EINVAL;

    memset(mutex, 0, sizeof(pthread_mutex_t));
    return 0;
}

static NACKED int __pthread_yield(void)
{
    SYSCALL(PTHREAD_YIELD);
}

int pthread_yield(void)
{
    return set_error(__pthread_yield());
}

static NACKED int __pthread_kill(pthread_t thread, int sig)
{
    SYSCALL(PTHREAD_KILL);
}

int pthread_kill(pthread_t thread, int sig)
{
    return set_error(__pthread_kill(thread, sig));
}

static NACKED void __pthread_exit(void *retval)
{
    SYSCALL(PTHREAD_EXIT);
}

/* The other way out of a thread, and the destructors are owed either way */
void pthread_exit(void *retval)
{
    __run_tls_destructors();
    __pthread_exit(retval);
}

static NACKED int __pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_UNLOCK);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return set_error(__pthread_mutex_unlock(mutex));
}

static NACKED int __pthread_mutex_lock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_LOCK);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return set_error(__pthread_mutex_lock(mutex));
}

static NACKED int __pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_TRYLOCK);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return set_error(__pthread_mutex_trylock(mutex));
}

static NACKED int __pthread_mutex_timedlock(pthread_mutex_t *mutex,
                                            const struct timespec *abstime)
{
    SYSCALL(PTHREAD_MUTEX_TIMEDLOCK);
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex,
                            const struct timespec *abstime)
{
    return set_error(__pthread_mutex_timedlock(mutex, abstime));
}
int pthread_condattr_init(pthread_condattr_t *attr)
{
    if (!attr)
        return EINVAL;

    /* No attribute is currently implemented */

    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr)
{
    if (!attr)
        return EINVAL;

    memset(attr, 0, sizeof(pthread_condattr_t));
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
    if (!cond)
        return EINVAL;

    INIT_LIST_HEAD(&((struct cond *) cond)->task_wait_list);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
    if (!cond)
        return EINVAL;

    memset(cond, 0, sizeof(pthread_cond_t));
    return 0;
}

static NACKED int __pthread_cond_signal(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_SIGNAL);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
    return set_error(__pthread_cond_signal(cond));
}

static NACKED int __pthread_cond_broadcast(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_BROADCAST);
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
    return set_error(__pthread_cond_broadcast(cond));
}

/* The mutex and the two waiting places inside are reached through the types a
 * program uses, so that the calls below are the ones a program would write
 */
static struct rwlock *rwlock_of(pthread_rwlock_t *rwlock)
{
    return (struct rwlock *) rwlock;
}

static pthread_mutex_t *rwlock_mutex(struct rwlock *rw)
{
    return (pthread_mutex_t *) &rw->lock;
}

/* Tenok reads nothing out of these: a read-write lock of it is always one
 * that the threads of the running task share and nothing else can reach
 */
int pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
    if (!attr)
        return EINVAL;

    *attr = 0;

    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
    if (!attr)
        return EINVAL;

    return 0;
}

int pthread_rwlock_init(pthread_rwlock_t *rwlock,
                        const pthread_rwlockattr_t *attr)
{
    if (!rwlock)
        return EINVAL;

    struct rwlock *rw = rwlock_of(rwlock);

    __mutex_init(&rw->lock);
    INIT_LIST_HEAD(&rw->readable.task_wait_list);
    INIT_LIST_HEAD(&rw->writable.task_wait_list);
    rw->readers = 0;
    rw->writers_waiting = 0;
    rw->writing = false;

    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
    if (!rwlock)
        return EINVAL;

    memset(rwlock, 0, sizeof(pthread_rwlock_t));

    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
    if (!rwlock)
        return EINVAL;

    struct rwlock *rw = rwlock_of(rwlock);
    pthread_mutex_t *mutex = rwlock_mutex(rw);

    pthread_mutex_lock(mutex);

    /* A writer already waiting is let in first, so that readers arriving one
     * after another cannot keep it out forever
     */
    while (rw->writing || rw->writers_waiting > 0)
        pthread_cond_wait((pthread_cond_t *) &rw->readable, mutex);

    rw->readers++;

    pthread_mutex_unlock(mutex);

    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
    if (!rwlock)
        return EINVAL;

    struct rwlock *rw = rwlock_of(rwlock);
    pthread_mutex_t *mutex = rwlock_mutex(rw);
    int retval = 0;

    pthread_mutex_lock(mutex);

    if (rw->writing || rw->writers_waiting > 0)
        retval = EBUSY;
    else
        rw->readers++;

    pthread_mutex_unlock(mutex);

    return retval;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
    if (!rwlock)
        return EINVAL;

    struct rwlock *rw = rwlock_of(rwlock);
    pthread_mutex_t *mutex = rwlock_mutex(rw);

    pthread_mutex_lock(mutex);

    /* Counted as waiting before waiting, so that readers arriving in the
     * meantime queue up behind rather than in front
     */
    rw->writers_waiting++;

    while (rw->writing || rw->readers > 0)
        pthread_cond_wait((pthread_cond_t *) &rw->writable, mutex);

    rw->writers_waiting--;
    rw->writing = true;

    pthread_mutex_unlock(mutex);

    return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
    if (!rwlock)
        return EINVAL;

    struct rwlock *rw = rwlock_of(rwlock);
    pthread_mutex_t *mutex = rwlock_mutex(rw);
    int retval = 0;

    pthread_mutex_lock(mutex);

    if (rw->writing || rw->readers > 0)
        retval = EBUSY;
    else
        rw->writing = true;

    pthread_mutex_unlock(mutex);

    return retval;
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
    if (!rwlock)
        return EINVAL;

    struct rwlock *rw = rwlock_of(rwlock);
    pthread_mutex_t *mutex = rwlock_mutex(rw);
    int retval = 0;

    pthread_mutex_lock(mutex);

    if (rw->writing)
        rw->writing = false;
    else if (rw->readers > 0)
        rw->readers--;
    else
        retval = EPERM;

    if (retval == 0) {
        /* The last one out lets a writer in if any is waiting, and every
         * reader in otherwise. Only one writer can be let in, where every
         * reader can go at once
         */
        if (rw->readers == 0 && rw->writers_waiting > 0)
            pthread_cond_signal((pthread_cond_t *) &rw->writable);
        else if (rw->writers_waiting == 0)
            pthread_cond_broadcast((pthread_cond_t *) &rw->readable);
    }

    pthread_mutex_unlock(mutex);

    return retval;
}

static NACKED int __pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_COND_WAIT);
}

/* The mutex is taken again before this returns, which is what lets the
 * handlers a cancelled thread runs find what it was holding still held
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    int ret = set_error(__pthread_cond_wait(cond, mutex));

    pthread_testcancel();

    return ret;
}

static NACKED int __pthread_cond_timedwait(pthread_cond_t *cond,
                                           pthread_mutex_t *mutex,
                                           const struct timespec *abstime)
{
    SYSCALL(PTHREAD_COND_TIMEDWAIT);
}

int pthread_cond_timedwait(pthread_cond_t *cond,
                           pthread_mutex_t *mutex,
                           const struct timespec *abstime)
{
    int ret = set_error(__pthread_cond_timedwait(cond, mutex, abstime));

    pthread_testcancel();

    return ret;
}

static NACKED int __pthread_once_begin(pthread_once_t *once_control)
{
    SYSCALL(PTHREAD_ONCE_BEGIN);
}

static NACKED int __pthread_once_end(pthread_once_t *once_control)
{
    SYSCALL(PTHREAD_ONCE_END);
}

/* The routine belongs to the caller and is called from here, where the caller
 * is, rather than from the kernel. The kernel only says which thread is to
 * call it, and hears back when it has been called
 */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    if (!once_control || !init_routine)
        return EINVAL;

    if (__pthread_once_begin(once_control) == 0) {
        init_routine();
        __pthread_once_end(once_control);
    }

    return 0;
}

static NACKED int __pthread_key_create(pthread_key_t *key,
                                       void (*destructor)(void *value))
{
    SYSCALL(PTHREAD_KEY_CREATE);
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *value))
{
    return set_error(__pthread_key_create(key, destructor));
}

static NACKED int __pthread_key_delete(pthread_key_t key)
{
    SYSCALL(PTHREAD_KEY_DELETE);
}

int pthread_key_delete(pthread_key_t key)
{
    return set_error(__pthread_key_delete(key));
}

static NACKED int __pthread_setspecific(pthread_key_t key, const void *value)
{
    SYSCALL(PTHREAD_SETSPECIFIC);
}

int pthread_setspecific(pthread_key_t key, const void *value)
{
    return set_error(__pthread_setspecific(key, value));
}

NACKED void *pthread_getspecific(pthread_key_t key)
{
    SYSCALL(PTHREAD_GETSPECIFIC);
}

static NACKED void *__pthread_next_destructor(void **value)
{
    SYSCALL(PTHREAD_NEXT_DESTRUCTOR);
}

/* Run in the thread that is ending, which is what POSIX asks for and what the
 * kernel cannot do for it. The kernel hands over one destructor at a time and
 * says so when there are none left; a destructor that leaves a new value
 * behind brings back another, so the turns are counted and given up on
 */
void __run_tls_destructors(void)
{
    int turns = PTHREAD_KEYS_MAX * PTHREAD_DESTRUCTOR_ITERATIONS;
    void (*destructor)(void *value);
    void *value;

    while (turns-- > 0) {
        destructor = __pthread_next_destructor(&value);
        if (!destructor)
            return;

        destructor(value);
    }
}

static NACKED int __pthread_setcancelstate(int state, int *oldstate)
{
    SYSCALL(PTHREAD_SETCANCELSTATE);
}

static NACKED int __pthread_testcancel(void)
{
    SYSCALL(PTHREAD_TESTCANCEL);
}

static NACKED int __cleanup_push(struct __pthread_cleanup *node)
{
    SYSCALL(PTHREAD_CLEANUP_PUSH);
}

static NACKED int __cleanup_pop(struct __pthread_cleanup *node)
{
    SYSCALL(PTHREAD_CLEANUP_POP);
}

static NACKED void *__next_cleanup(void **arg)
{
    SYSCALL(PTHREAD_NEXT_CLEANUP);
}

void __pthread_cleanup_push(struct __pthread_cleanup *node,
                            void (*routine)(void *arg),
                            void *arg)
{
    node->routine = routine;
    node->arg = arg;
    __cleanup_push(node);
}

void __pthread_cleanup_pop(struct __pthread_cleanup *node, int execute)
{
    if (__cleanup_pop(node) != 0)
        return;

    if (execute)
        node->routine(node->arg);
}

int pthread_setcancelstate(int state, int *oldstate)
{
    int retval = -__pthread_setcancelstate(state, oldstate);
    if (retval != 0)
        return retval;

    /* A request that arrived while it was turned off is let in now */
    if (state == PTHREAD_CANCEL_ENABLE)
        pthread_testcancel();

    return 0;
}

/* Tenok stops a thread only where stopping is safe, which is what POSIX calls
 * deferred. Doing it anywhere would mean the kernel breaking into a thread
 * wherever it happened to be, and almost nothing a thread does is safe to be
 * broken into
 */
int pthread_setcanceltype(int type, int *oldtype)
{
    if (type != PTHREAD_CANCEL_DEFERRED && type != PTHREAD_CANCEL_ASYNCHRONOUS)
        return EINVAL;

    if (oldtype)
        *oldtype = PTHREAD_CANCEL_DEFERRED;

    return type == PTHREAD_CANCEL_DEFERRED ? 0 : ENOTSUP;
}

/* Where a thread stops if it was asked to. The handlers it pushed are run
 * here, in the thread that pushed them, and pthread_exit() sees to the rest:
 * the values it left under its keys, and the answer it leaves for a join
 */
void pthread_testcancel(void)
{
    if (!__pthread_testcancel())
        return;

    void (*routine)(void *arg);
    void *arg;

    while ((routine = __next_cleanup(&arg)))
        routine(arg);

    pthread_exit(PTHREAD_CANCELED);
}

/* Tenok has no fork() for these to be run either side of, so there is never
 * anything to remember
 */
int pthread_atfork(void (*prepare)(void),
                   void (*parent)(void),
                   void (*child)(void))
{
    return ENOSYS;
}
