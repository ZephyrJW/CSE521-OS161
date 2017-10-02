#include <syscall.h>
#include <file_syscalls.h>
#include <lib.h>
#include <vfs.h>
#include <vnode.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <limits.h>
#include <uio.h>
#include <kern/iovec.h>
#include <synch.h>
#include <current.h>
#include <copyinout.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <stdarg.h>
#include <kern/seek.h>

// asst2.1 include >?
#include <test.h>
#include <proc.h>

int
filetable_init(void)
{
	struct vnode *vstdin;
	struct vnode *vstdout;
	struct vnode *vstderr;
	char *vin = NULL;
	char *vout = NULL;
	char *verr = NULL;

	vin = kstrdup("con:");
	vout = kstrdup("con:");
	verr = kstrdup("con:");

	int result = 0;

	result = vfs_open(vin, O_RDONLY, 0, &vstdin);
	if(result){
		kfree(vin);
		kfree(vout);
		kfree(verr);
		vfs_close(vstdout);
		return EINVAL;
	}
	curthread->fdtable[0] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
//	fdtable[0] = kmalloc(sizeof(struct ftunit));
	strcpy(curthread->fdtable[0]->filename, vin);
	curthread->fdtable[0]->file = vstdin;
	curthread->fdtable[0]->refcount = 1;
	curthread->fdtable[0]->fhlock = lock_create(vin);
	curthread->fdtable[0]->offset = 0;
	curthread->fdtable[0]->openflags = O_RDONLY;

	result = vfs_open(vout, O_WRONLY, 0, &vstdout);
	if(result){
		lock_destroy(curthread->fdtable[0]->fhlock);
		kfree(curthread->fdtable[0]);
		kfree(vin);
		kfree(vout);
		kfree(verr);
		vfs_close(vstdin);
		vfs_close(vstdout);
		return EINVAL;
	}
	curthread->fdtable[1] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
	strcpy(curthread->fdtable[1]->filename, vout);
	curthread->fdtable[1]->file = vstdout;
	curthread->fdtable[1]->refcount = 1;
	curthread->fdtable[1]->fhlock = lock_create(vout);
	curthread->fdtable[1]->offset = 0;
	curthread->fdtable[1]->openflags = O_WRONLY;

	result = vfs_open(verr, O_WRONLY, 0, &vstderr);
	if(result){
		lock_destroy(curthread->fdtable[0]->fhlock);
		kfree(curthread->fdtable[0]);
		lock_destroy(curthread->fdtable[1]->fhlock);
		kfree(curthread->fdtable[0]);
		kfree(vin);
		kfree(vout);
		kfree(verr);
		vfs_close(vstdin);
		vfs_close(vstdout);
		vfs_close(vstderr);
		return EINVAL;
	}
	curthread->fdtable[2] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
	strcpy(curthread->fdtable[2]->filename, verr);
	curthread->fdtable[2]->file = vstderr;
	curthread->fdtable[2]->refcount = 1;
	curthread->fdtable[2]->fhlock = lock_create(verr);
	curthread->fdtable[2]->offset = 0;
	curthread->fdtable[2]->openflags = O_WRONLY;

	/*clean up*/
	kfree(vin);
	kfree(vout);
	kfree(verr);

	return 0;
}

/*
int
filetable_init(void)
{
	struct vnode *vstdin;
	struct vnode *vstdout;
	struct vnode *vstderr;
	char *vin = NULL;
	char *vout = NULL;
	char *verr = NULL;

	vin = kstrdup("con:");
	vout = kstrdup("con:");
	verr = kstrdup("con:");

	int result = 0;

	result = vfs_open(vin, O_RDONLY, 0, &vstdin);
	if(result){
		kfree(vin);
		kfree(vout);
		kfree(verr);
		vfs_close(vstdout);
		return EINVAL;
	}
	curthread->fdtable[0] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
	strcpy(curthread->fdtable[0]->filename, vin);
	curthread->fdtable[0]->file = vstdin;
	curthread->fdtable[0]->refcount = 1;
	curthread->fdtable[0]->fhlock = lock_create(vin);
	curthread->fdtable[0]->offset = 0;
	curthread->fdtable[0]->openflags = O_RDONLY;

	result = vfs_open(vout, O_WRONLY, 0, &vstdout);
	if(result){
		lock_destroy(curthread->fdtable[0]->fhlock);
		kfree(curthread->fdtable[0]);
		kfree(vin);
		kfree(vout);
		kfree(verr);
		vfs_close(vstdin);
		vfs_close(vstdout);
		return EINVAL;
	}
	curthread->fdtable[1] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
	strcpy(curthread->fdtable[1]->filename, vout);
	curthread->fdtable[1]->file = vstdout;
	curthread->fdtable[1]->refcount = 1;
	curthread->fdtable[1]->fhlock = lock_create(vout);
	curthread->fdtable[1]->offset = 0;
	curthread->fdtable[1]->openflags = O_WRONLY;

	result = vfs_open(verr, O_WRONLY, 0, &vstderr);
	if(result){
		lock_destroy(curthread->fdtable[0]->fhlock);
		kfree(curthread->fdtable[0]);
		lock_destroy(curthread->fdtable[1]->fhlock);
		kfree(curthread->fdtable[0]);
		kfree(vin);
		kfree(vout);
		kfree(verr);
		vfs_close(vstdin);
		vfs_close(vstdout);
		vfs_close(vstderr);
		return EINVAL;
	}
	curthread->fdtable[2] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
	strcpy(curthread->fdtable[2]->filename, verr);
	curthread->fdtable[2]->file = vstderr;
	curthread->fdtable[2]->refcount = 1;
	curthread->fdtable[2]->fhlock = lock_create(verr);
	curthread->fdtable[2]->offset = 0;
	curthread->fdtable[2]->openflags = O_WRONLY;

	return 0;
}
*/


int
sys_open(const char *filename, int flags, int *retval){
	int result = 0, index = 3;
	struct vnode *v;
	char *tmp;
	tmp = (char *)kmalloc(sizeof(char) * PATH_MAX);
	size_t len;
	result = copyinstr((const_userptr_t)filename, tmp, PATH_MAX, &len);
	if(result){
		kfree(tmp);
		return result;
	}

	while(index < OPEN_MAX){
		if(curthread->fdtable[index] != NULL){
			index++;
		}else{
			break;
		}
	}

	if(index == OPEN_MAX){
		kfree(tmp);
		return ENFILE;
	}

	result = vfs_open(tmp, flags, 0, &v);
	if(result){
		kfree(tmp);
		return result;
	}

	curthread->fdtable[index] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
	if(curthread->fdtable[index] == NULL){
		vfs_close(v);
		kfree(tmp);
		return ENFILE;	
	}
	strcpy(curthread->fdtable[index]->filename, tmp);
	curthread->fdtable[index]->file = v;
	curthread->fdtable[index]->refcount = 1;
	curthread->fdtable[index]->fhlock = lock_create(tmp);
	curthread->fdtable[index]->offset = 0;
	curthread->fdtable[index]->openflags = flags;

	kfree(tmp);
	*retval = index;

	return 0;
}

ssize_t
sys_read(int fd, void *buf, size_t buflen, int *retval){
	if(fd >= OPEN_MAX || fd < 0){
		return EBADF;
	}

	if(curthread->fdtable[fd] == NULL){
		return EBADF;
	}

	if((curthread->fdtable[fd]->openflags & 3) == O_WRONLY){
		return EBADF;
	}
/*
	if(buf == NULL){
		return EINVAL;
	}
*/

	int result = 0;
	void *tmp;
	tmp = kmalloc(sizeof(char) * buflen);
	result = copyin((const_userptr_t)buf, tmp, buflen);
	if(result){
		kfree(tmp);
		return result;
	}
	kfree(tmp);

	struct iovec iov;
	struct uio u;

	lock_acquire(curthread->fdtable[fd]->fhlock);

	uio_kinit(&iov, &u, buf, buflen, curthread->fdtable[fd]->offset, UIO_READ);

	result = VOP_READ(curthread->fdtable[fd]->file, &u);
	if(result){
		lock_release(curthread->fdtable[fd]->fhlock);
		return result;
	}

	curthread->fdtable[fd]->offset = u.uio_offset;
	*retval = buflen - u.uio_resid;
	lock_release(curthread->fdtable[fd]->fhlock);

	return 0;
}


int
sys_write(int fd, void* buf, size_t size, int *retval)
{
	if(fd >= OPEN_MAX || fd < 0){
		return EBADF;
	}

	if(curthread->fdtable[fd] == NULL){
		return EBADF;
	}

	// and 1 equals 0 , means the last bit is 0, which indicated O_RDONLY
	// however, it overlooks O_RDWR, hence we should reserve 2 bit info
	if((curthread->fdtable[fd]->openflags & 3) == O_RDONLY){
		return EBADF;
	}


	struct iovec iov;
	struct uio u;
	int result = 0;

	void *tmp;
	tmp = kmalloc(sizeof(char) * size);
	if(tmp == NULL) {
		return EINVAL;
	}

	result = copyin((const_userptr_t)buf, tmp, size);
	if(result) {
		kfree(tmp);
		return result;
	}

	lock_acquire(curthread->fdtable[fd]->fhlock);

	/*debug
	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = size;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = curthread->fdtable[fd]->offset;
	u.uio_resid = size;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_WRITE;
	u.uio_space = proc_getas();
	if(proc_getas() == NULL){
		kprintf("getas() returns NULL\n");
	}
	*/
	uio_kinit(&iov, &u, tmp, size, curthread->fdtable[fd]->offset, UIO_WRITE);

	result = VOP_WRITE(curthread->fdtable[fd]->file, &u);
	if(result){
		kfree(tmp);
		lock_release(curthread->fdtable[fd]->fhlock);
		return result;
	}
	kfree(tmp);
	curthread->fdtable[fd]->offset = u.uio_offset;
	// uio_resid means how much need to be written.
	// Q: can we simpy retval = uio_offset
	*retval = size - u.uio_resid;

	lock_release(curthread->fdtable[fd]->fhlock);
	return 0;
}

int
sys_close(int fd){
	if(fd >= OPEN_MAX || fd < 0){
		return EBADF;
	}

	if(curthread->fdtable[fd] == NULL){
		return EBADF;
	}
// is it required? if file == NULL, we can't allocate this fhandle??
	if(curthread->fdtable[fd]->file == NULL){
		return EBADF;
	}

	lock_acquire(curthread->fdtable[fd]->fhlock);

	if(curthread->fdtable[fd]->refcount == 1){
		// is this alright??
		vfs_close(curthread->fdtable[fd]->file);
		lock_release(curthread->fdtable[fd]->fhlock);
		lock_destroy(curthread->fdtable[fd]->fhlock);
		//kfree(curthread->fdtable[fd]->filename);
		kfree(curthread->fdtable[fd]);
		curthread->fdtable[fd] = NULL;
	}else{
		curthread->fdtable[fd]->refcount--;
		lock_release(curthread->fdtable[fd]->fhlock);
		curthread->fdtable[fd] = NULL;
	}

	return 0;
}


int
sys_lseek(int fd, off_t pos, int whence, int* retval, int* retval_64)
{
	//check
	if (fd >= OPEN_MAX || fd < 0) return EBADF;

	if (curthread->fdtable[fd] == NULL) return EBADF;

	//if (fd < 3) return ESPIPE;

	lock_acquire(curthread->fdtable[fd]->fhlock);

	int result = 0;

	struct stat statval;
	result = VOP_STAT(curthread->fdtable[fd]->file, &statval);
	if (result) {
		lock_release(curthread->fdtable[fd]->fhlock);
		return result;
	}
	off_t fsize = statval.st_size;

	result = VOP_ISSEEKABLE(curthread->fdtable[fd]->file);

	if (!result) {
		lock_release(curthread->fdtable[fd]->fhlock);
		return ESPIPE;
	}

	off_t retpos;
	if (whence == SEEK_SET) retpos = pos;
	else if (whence == SEEK_CUR) retpos = curthread->fdtable[fd]->offset + pos;
	else if (whence == SEEK_END) retpos = pos + fsize;
	else {
		lock_release(curthread->fdtable[fd]->fhlock);
		return EINVAL;
	}

	if (retpos < 0) {
		lock_release(curthread->fdtable[fd]->fhlock);
		return EINVAL;
	}


	curthread->fdtable[fd]->offset = retpos;
	*retval = (uint32_t) (retpos >> 32);
	*retval_64 = (uint32_t) retpos;

	lock_release(curthread->fdtable[fd]->fhlock);

	return 0;
}

int
sys_dup2(int oldfd, int newfd, int *retval){
	if(oldfd >= OPEN_MAX || oldfd < 0 || newfd >= OPEN_MAX || newfd < 0){
		return EBADF;
	}

	if(curthread->fdtable[oldfd] == NULL){
		return EBADF;
	}

	if(oldfd == newfd){
		*retval = oldfd;
		return 0;
	}

	int result = 0;

	if(curthread->fdtable[newfd] != NULL){
		result = sys_close(newfd);
		if(result) return result;
		curthread->fdtable[newfd] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
	}else{
		curthread->fdtable[newfd] = (struct ftunit *)kmalloc(sizeof(struct ftunit));
	}

	lock_acquire(curthread->fdtable[oldfd]->fhlock);

	curthread->fdtable[newfd] = curthread->fdtable[oldfd];
	curthread->fdtable[newfd]->refcount++;
/*
	strcpy(curthread->fdtable[oldfd]->filename, curthread->fdtable[newfd]->filename);
	curthread->fdtable[newfd]->file = curthread->fdtable[oldfd]->file;
	curthread->fdtable[newfd]->refcount = curthread->fdtable[oldfd]->refcount;//1;
	curthread->fdtable[newfd]->fhlock = lock_create("duplication");
	curthread->fdtable[newfd]->offset = curthread->fdtable[oldfd]->offset;
	curthread->fdtable[newfd]->openflags = curthread->fdtable[oldfd]->openflags;
*/

	lock_release(curthread->fdtable[oldfd]->fhlock);

	*retval = newfd;
	return 0;
}




int
sys_chdir(const char *pathname){
	char *tmp;
	tmp = (char *)kmalloc(sizeof(char) * PATH_MAX);
	// int ,, size_t: unsigned, ssize_t: signed
	int result = 0;
	size_t len;
	result = copyinstr((const_userptr_t) pathname, tmp, sizeof(pathname), &len);
	if(result){
		kfree(tmp);
		return result;
	}

	result = vfs_chdir(tmp);
	if(result){
		kfree(tmp);
		return result;
	}
	kfree(tmp);

	return 0;
}

int
sys___getcwd(char *buf, size_t buflen, int *retval){
	int result = 0;
	char *tmp;
	tmp = (char *)kmalloc(sizeof(char)*PATH_MAX);

	size_t got;
	result = copyinstr((const_userptr_t )buf, tmp, buflen, &got);
	if(result){
		kfree(tmp);
		return result;
	}
	kfree(tmp);

	struct iovec iov;
	struct uio u;

	uio_kinit(&iov, &u, buf, buflen, 0, UIO_READ);

	result = vfs_getcwd(&u);
	if(result){
		return result;
	}
	buf[buflen - u.uio_resid + 1] = '\0';

	*retval = strlen(buf);

	return 0;
}
