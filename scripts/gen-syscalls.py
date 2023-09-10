#!/usr/bin/env python

syscalls = \
['set_irq',
 'set_program_name',
 'sched_yield',
 'fork',
 'sleep',
 'mount',
 'open',
 'close',
 'read',
 'write',
 'lseek',
 'fstat',
 'opendir',
 'readdir',
 'getpriority',
 'setpriority',
 'getpid',
 'mknod',
 'mkfifo',
 'mq_open',
 'mq_receive',
 'mq_send',
 'pthread_mutex_init',
 'pthread_mutex_unlock',
 'pthread_mutex_lock',
 'pthread_cond_init',
 'pthread_cond_signal',
 'pthread_cond_wait',
 'sem_init',
 'sem_post',
 'sem_trywait',
 'sem_wait',
 'sem_getvalue']

syscall_cnt = len(syscalls)

import datetime
print('// GENERATED. DO NOT EDIT FROM HERE!')
print('// Change definitions in scripts/gen-syscalls.py')
print('// Created on ' +
    datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))

print('')

print('#ifndef __SYSCALL_H__')
print('#define __SYSCALL_H__\n')

for i in range(0, syscall_cnt):
    id = syscalls[i].upper()
    syscall_num = i + 1
    print('#define %s %d' %(id, syscall_num))

print('\n#define SYSCALL_TABLE_INIT \\');
for i in range(0, syscall_cnt):
    syscall = syscalls[i]
    id = syscalls[i].upper()
    if i == syscall_cnt - 1:
        print('    DEF_SYSCALL(%s, %s)\n' %(syscall, id))
    else:
        print('    DEF_SYSCALL(%s, %s), \\' %(syscall, id))

for i in range(0, syscall_cnt):
    print("void sys_%s(void);" %(syscalls[i]))

print('\n#endif')
