#ifndef _PROC_SYSCALLS_H_
#define _PROC_SYSCALLS_H_

#include <types.h>
#include <limits.h>

int 
sys_getpid(int *retval);


void entry_point(void *data1, unsigned long data2);

int 
sys_fork(struct trapframe* tf, int* retval);

int 
sys_waitpid(pid_t pid, int *status, int options, int *retval);

void 
sys__exit(int exitcode, bool trap);


int  
sys_execv(const char *program, char **args);

int
//void *
sys_sbrk(intptr_t amount, vaddr_t *retval);

#endif
