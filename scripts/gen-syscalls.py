#!/usr/bin/env python

import datetime

syscalls = \
    ['procstat',
     'setprogname',
     'getprogname',
     'delay_ticks',
     'task_create',
     'mpool_alloc',
     'minfo',
     'sched_yield',
     'exit',
     'mount',
     'open',
     'close',
     'dup',
     'dup2',
     'read',
     'write',
     'ioctl',
     'lseek',
     'fstat',
     'opendir',
     'readdir',
     'getpid',
     'gettid',
     'mknod',
     'mkfifo',
     'poll',
     'mq_getattr',
     'mq_setattr',
     'mq_open',
     'mq_close',
     'mq_unlink',
     'mq_receive',
     'mq_send',
     'pthread_create',
     'pthread_join',
     'pthread_detach',
     'pthread_cancel',
     'pthread_setschedparam',
     'pthread_getschedparam',
     'pthread_yield',
     'pthread_kill',
     'pthread_exit',
     'pthread_mutex_unlock',
     'pthread_mutex_lock',
     'pthread_mutex_trylock',
     'pthread_cond_signal',
     'pthread_cond_broadcast',
     'pthread_cond_wait',
     'pthread_once',
     'sem_post',
     'sem_trywait',
     'sem_wait',
     'sem_getvalue',
     'sigaction',
     'sigwait',
     'kill',
     'raise',
     'clock_gettime',
     'clock_settime',
     'timer_create',
     'timer_delete',
     'timer_settime',
     'timer_gettime',
     'malloc',
     'free']

reserved_events = [
    'SIGNAL_CLEANUP_EVENT',
    'THREAD_RETURN_EVENT',
    'THREAD_ONCE_EVENT']

syscall_cnt = len(syscalls)
reserved_events_cnt = len(reserved_events)

print('// GENERATED. DO NOT EDIT FROM HERE!')
print('// Change definitions in scripts/gen-syscalls.py')
print('// Created on ' +
      datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))

print('')

print('/** @file */')
print('#ifndef __KERNEL_SYSCALL_H__')
print('#define __KERNEL_SYSCALL_H__\n')

print('#define SYSCALL_CNT %d\n' % (syscall_cnt))

for i in range(0, syscall_cnt):
    id = syscalls[i].upper()
    syscall_num = i + 1
    print('#define %s %d' % (id, syscall_num))

print('')
for i in range(0, reserved_events_cnt):
    event = reserved_events[i]
    value = i + syscall_cnt + 1
    print('#define %s %d' % (event, value))

print('\n#define SYSCALL_TABLE_INIT { \\')

for i in range(0, syscall_cnt):
    syscall = syscalls[i]
    id = syscalls[i].upper()
    if i == syscall_cnt - 1:
        print('    DEF_SYSCALL(%s, %s) \\\n    }\n' % (syscall, id))
    else:
        print('    DEF_SYSCALL(%s, %s), \\' % (syscall, id))

for i in range(0, syscall_cnt):
    print("void sys_%s(void);" % (syscalls[i]))

print('\n#endif')
