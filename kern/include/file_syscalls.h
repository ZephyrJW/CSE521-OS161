#ifndef FILE_SYSCALLS_H_
#define FILE_SYSCALLS_H_

#include <types.h>
#include <limits.h>

struct ftunit{
	char filename[NAME_MAX];	
	struct vnode *file;
	int refcount; // reference count.
	struct lock *fhlock;	

	off_t offset;
	int openflags;
};

// --!-- ftable_init
int
//filetable_init(void);
filetable_init(void);

int
sys_open(const char *filename, int flags, int *retval);

ssize_t 
sys_read(int fd, void *buf, size_t buflen,int *retval);

ssize_t
sys_write(int handle, void *buffer, size_t nbytes, int *retval);

int 
sys_close(int fd);

int
sys_lseek(int fd, off_t pos, int whence, int* retval, int* retval_64);

int 
sys_dup2(int oldfd, int newfd, int *retval);

int 
sys_chdir(const char *pathname);

int 
sys___getcwd(char *buf, size_t buflen, int *retval);

#endif
