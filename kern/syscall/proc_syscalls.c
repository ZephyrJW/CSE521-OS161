#include <types.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/wait.h>
#include <addrspace.h>
#include <synch.h>
#include <current.h>
#include <copyinout.h>
#include <vfs.h>
#include <syscall.h>
#include <proc_syscalls.h>
#include <proc.h>
#include <file_syscalls.h>
#include <vnode.h>
#include <addrspace.h>
#include <syscall.h>
#include <vm.h>

#include <mips/tlb.h>
#include <spl.h>
#include <test.h>

#if 1
int  
sys_getpid(int *retval){
	*retval = curproc->pid;
	return 0;
}

void entry_point(void *data1, unsigned long data2){
	struct trapframe tmp;
	struct trapframe *childtf = (struct trapframe *)data1;
	struct addrspace *childas = (struct addrspace *)data2;
	
	childtf->tf_v0 = 0;
	childtf->tf_a3 = 0;
	childtf->tf_epc += 4;

	memcpy(&tmp, childtf, sizeof(struct trapframe));
	kfree(childtf);
	childtf = NULL;
	
	curproc->p_addrspace = childas;
	as_activate();	
		
	mips_usermode(&tmp);
}


int 
sys_fork(struct trapframe* tf, int* retval)
{
//	kprintf("fork\n");
	struct trapframe* child_tf = NULL;
	struct proc* child_proc = NULL;
	struct addrspace* child_addr = NULL;

	int result;

	child_tf = (struct trapframe *)kmalloc(sizeof(struct trapframe));
	if (child_tf == NULL) {
		*retval = -1;
		return ENOMEM;
	}

	memcpy(child_tf, tf, sizeof(struct trapframe));

	result = as_copy(curproc->p_addrspace, &child_addr);

	if(result){
		kfree(child_tf);
		return ENOMEM;	
	}

	if (child_addr == NULL) {
		kfree(child_tf);
		//*retval = -1;
		return ENOMEM;
	}
//	kprintf("%d", (int)curproc->p_addrspace);

	child_proc = proc_create("child");
	
	if (child_proc == NULL) {
		kfree(child_tf);
		as_destroy(child_addr); // alright? 
		//*retval = -1;
		return ENOMEM;
	}

	child_proc->ppid = curproc->pid;	

	/*fdtable
	for (int fd = 0; fd<OPEN_MAX; fd++){
		child_proc->fdtable[fd] = curproc->fdtable[fd];
		if(child_proc->fdtable[fd] != NULL){
			child_proc->fdtable[fd]->refcount++;	
		}	
	}
	*/

	/*file table copy is done inside thread_fork()*/
	result = thread_fork("process", child_proc, entry_point, child_tf, (unsigned long)child_addr);

	if (result) {
		return result;
	}

	*retval = child_proc->pid;
	curproc->p_numthreads++;
	// jzhao44?
	child_proc->p_cwd = curproc->p_cwd;
	//VOP_INCREF(curproc->p_cwd);
	return 0;
}

int 
sys_waitpid(pid_t pid, int *status, int options, int *retval){
	if(status != NULL){
		if(status == ((int *)0x40000000) || status == ((int *)0x80000000) || ((int)status & 3) != 0){
			return EFAULT;
		}
	}

	//jzhao44
	//if(status == NULL) kprintf("status == NULL comes to here --1\n");

	if(options != 0){
		return EINVAL;	
	}

	//jzhao44
	//if(status == NULL) kprintf("status == NULL comes to here --2\n");


	if(pid < PID_MIN || pid >= PID_MAX){
		return ESRCH;	
	}

	//jzhao44
	//if(status == NULL) kprintf("status == NULL comes to here --3\n");

	struct proc *p = proc_table[pid];

	if(p == NULL) return ESRCH;

	//jzhao44
	//if(status == NULL) kprintf("status == NULL comes to here!\n");

	if(curproc->pid != p->ppid) return ECHILD;

	lock_acquire(p->waitp_lk);
	//if(lock_do_i_hold()) printf("im holding\n");
	while(p->exited == false){
		cv_wait(p->waitp, p->waitp_lk);	
	}
	
	lock_release(p->waitp_lk);
	if(status != NULL) *status = p->e_code;
	*retval = pid;

	spinlock_cleanup(&p->p_lock);
	lock_destroy(p->waitp_lk);
	cv_destroy(p->waitp);
	as_destroy(p->p_addrspace);
	p->p_addrspace = NULL;
	kfree(p->p_name);

	//kfree(proc_table[pid]);
	proc_table[pid] = NULL;
	kfree(p);
	p = NULL;
	//jzhao44
	//if(status == NULL) kprintf("also comes to this point!\n");
	
	return 0;
}


void 
sys__exit(int exitcode, bool trap){
	//kprintf("exit1\n");
	lock_acquire(curproc->waitp_lk);

	curproc->exited = true;
	if(trap == true){
		curproc->e_code = _MKWAIT_SIG(exitcode);	
	}else{
		curproc->e_code = _MKWAIT_EXIT(exitcode);
	}
	for(int fd=0; fd< OPEN_MAX; fd++){
		//if(curthread->fdtable[fd] != NULL) sys_close(fd);
		sys_close(fd);
	}
	
	pid_t parent = curproc->ppid;
	//struct proc *e = proc_table[curproc->pid];
	
	if(proc_table[parent] != NULL && proc_table[parent]->exited == false){
		cv_broadcast(curproc->waitp, curproc->waitp_lk);	
		lock_release(curproc->waitp_lk);
	}else{
		//orphan, nobody will collect it, just remove.
		lock_release(curproc->waitp_lk);	
		lock_destroy(curproc->waitp_lk);
		spinlock_cleanup(&curproc->p_lock);
		cv_destroy(curproc->waitp);
		as_destroy(curproc->p_addrspace);
		curproc->p_addrspace = NULL;
		kfree(curproc->p_name);
		kfree(proc_table[curproc->pid]);
		kfree(curproc);
		
		proc_table[curproc->pid] = NULL;

	}
	thread_exit();
}

#if 1
int 
sys_execv(const char *program, char **args){
	// Invalid pointer arguments
	if(program == NULL || args == NULL) return EFAULT;	
	if((int *)args == (int *)0x40000000 || (int *)args == (int *)0x80000000) return EFAULT;

	int result = 0;
	// Count number of arguments in args
	int args_cnt = 0;
	//kprintf("userspace[0]: %s\n", args[0]);

	/*this very next line generate bus exception error with randcall.t
	  so the pointers in userspace is not safe, copyin is better.*/
	//while(args[args_cnt] != NULL) args_cnt++;

	//kprintf("args_cnt: %d\n", args_cnt);
	char **argptr = (char **)kmalloc(sizeof(char *) * (4 * 1024)/*From man page, minimum limitation*/);
	while(1){
		result = copyin((userptr_t)&(args[args_cnt]), &(argptr[args_cnt]), sizeof(char *));
		if(result) return result;		
		if(argptr[args_cnt++] == NULL) break;
	}
	args_cnt--;
	//kprintf("args_cnt: %d\n", args_cnt);


	// if not copyin, badcall may resultin empty string and exit code failure
	// copyin arg string
	char **argstr = (char **)kmalloc(sizeof(char *) * args_cnt);
	for(int i=0; i< args_cnt; i++){
		if((int *)(argptr[i]) == (int *)0x40000000 || (int *)(argptr[i]) == (int *)0x80000000) return EFAULT;
		int arglen = strlen(argptr[i]) + 1;
		if(arglen % 4 != 0) arglen += 4 - arglen%4;
		argstr[i] = (char *)kmalloc(sizeof(char) * arglen);	
		size_t len;
		result = copyinstr((userptr_t)argptr[i], argstr[i], arglen, &len);
		if(result) return result;
	}
/*
	for(int i = 0; i< 4*1024; i++){
		kprintf("%d\n", i);
		kfree(argptr + sizeof(char *) * i);	
	}
*/
	kfree(argptr);

	/*kprintf("~args_cnt: %d\n", args_cnt);
	for(int i=0;i<args_cnt;i++){
		//kprintf("args[%d]:%s\n", i, argstr[i]);	
	}
	*/

	int total = (args_cnt + 1) * 4; // args and a NULL 

	for(int i=0; i<args_cnt; i++){
		int arglen = strlen(argstr[i]) + 1;
		int tmp = arglen % 4;
		if(tmp != 0) tmp = 4 - tmp;
		
		total += arglen + tmp;	
	}

	if(total > ARG_MAX){
		return E2BIG;	
	}

	char **stack_addr = (char **)kmalloc(sizeof(char *) * (args_cnt + 1));
	int pos = (args_cnt + 1) * 4; 
	for(int i=0; i<args_cnt; i++){
		stack_addr[i] = (char *)pos;	

		int arglen = strlen(argstr[i]) + 1;
		if(arglen % 4 != 0) arglen = arglen + 4 - (arglen % 4);
		pos += arglen;
	}
	stack_addr[args_cnt] = NULL;

	//debug
	//kprintf("first two safe!\n");

	/*debug  argstr copy correctly..
	kprintf("args count: %d\n", args_cnt);
	for(int i=0;;i++){
		if(argstr[i] != NULL) kprintf("args[%d]: %s- %s    ", i, argstr[i], args[i]);	
		else break;
	}	
	kprintf("\ntotal len: %d\n", total);
	//debug end */

	//kprintf("args_cnt: %d\n", args_cnt);

	// Copyin programname
	char *progpath = (char *)kmalloc(sizeof(char) * PATH_MAX);
	size_t pathlen = 0;
	result = copyinstr((userptr_t)program, progpath, PATH_MAX, &pathlen);
	if(result) {
		kfree(progpath);
		return result;
	}
	if(strlen(progpath) == 0) {
		kfree(progpath);
		return EINVAL;
	}

	// runprogram part
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	
	result = vfs_open(progpath, O_RDONLY, 0, &v);
	if(result){
		kfree(progpath);	
		return result;
	}				

	as = proc_getas();
	as_destroy(as);	
	as = NULL;
	proc_setas(NULL);
	//KASSERT(proc_getas() == NULL);

	as = as_create();
	if(as == NULL){
		vfs_close(v);
		kfree(progpath);
		return ENOMEM;	
	}

	proc_setas(as);
	as_activate();

	result = load_elf(v, &entrypoint);
	if(result){
		vfs_close(v);
		kfree(progpath);
		return result;	
	}

	vfs_close(v);
	kfree(progpath);

	result = as_define_stack(as, &stackptr);
	if(result){
		return result;	
	}

	// copyout args to user stack
	stackptr -= total;
	// argv[0-argcnt] plus a NULL

	/*Q1: stackptr, when calling enter_new_process, where should it point to??*/

#if 1
	//kprintf("start copy out, %p, args_cnt :%d\n", (void *)(stackptr), args_cnt);
	//kprintf("argstr still here? %s\n", argstr[1]);
	int start = (args_cnt + 1) * 4;
	for(int i=0; i< args_cnt; i++){
		//kprintf("1   %s ", argstr[i]);
		int arglen = strlen(argstr[i]) + 1;
		//kprintf("2   \n");
		int tmp = arglen % 4;
		if(tmp) tmp = 4 - tmp;
		//kprintf("%d, %d+%d", i, arglen, tmp);

		//kprintf("stops here\n");
		stack_addr[i] += stackptr;
		result = copyout(&stack_addr[i], (userptr_t)(stackptr+ i*sizeof(char *)), sizeof(char *));
		if(result) return result;
		//kprintf("after copyout ptr: %p has ptr %p\n", (char *)(stackptr + i*(sizeof(char *))), &(char *)(stackptr+i*sizeof(char *)));

		//this value is correct. kprintf("copyout ptr[%d]: %p\n", i, stack_addr[i]+stackptr);
		result = copyout(argstr[i], (userptr_t)(stackptr + start), arglen);
		if(result) return result;

		//kprintf("copy out ptr: %p   ", (void *)(stack_addr[i] + stackptr));
		//kprintf("pointer[%d]: %p, actual[%d]: %p then +%d,	", i, ((char *)((stackptr+ i*sizeof(char *)))), i, (void *)(stackptr+start), arglen+tmp);
		//kprintf("stackspace[%d]: %s\n", i, (char *)(stackptr+start));

		start = start + arglen + tmp;
		//kprintf("also done summation\n");
	}
	//kprintf("first ptr[%p]: %p\n",(void *)stackptr , &stackptr);

	//result = copyout(stack_addr[args_cnt], (userptr_t)(stackptr+args_cnt*sizeof(char *)), sizeof(char *));	
	//if(result) return result;

#endif
	for(int i=0; i<args_cnt; i++){
		kfree(argstr[i]);	
	}

	kfree(argstr);
	kfree(stack_addr);

	enter_new_process(args_cnt, (userptr_t)stackptr, (userptr_t)stackptr, stackptr, entrypoint);

	panic("enter_new_process nerver return\n");

	return EINVAL;
}
#endif

int
//void *
sys_sbrk(intptr_t amount, vaddr_t *retval){
	/*page aligned amount??*/
	if(amount % PAGE_SIZE != 0){
		*retval = -1;
		return EINVAL;
	}

	struct addrspace *as;
	as = proc_getas();
	KASSERT(as != NULL);

	if((int)as->heap_pages + amount/PAGE_SIZE < 0){
		//kprintf("huge negative\n");
		*retval = -1;
		return EINVAL;
		//return ENOMEM;
	}

	if(as->heap_start + as->heap_pages * PAGE_SIZE + amount >= USERSTACK - VM_STACKPAGES * PAGE_SIZE){
		//kprintf("huge negative?\n"); here
		//if(amount < 0) kprintf("huge negative: %d  \n", (int)amount );
		*retval = -1;
		//return = EINVAL;
		return ENOMEM;
	}

	size_t npages = amount / PAGE_SIZE;
	*retval = as->heap_start + as->heap_pages * PAGE_SIZE;
	as->heap_pages += npages;
	
	if(amount < 0 && as->page_table != NULL){
		struct page_table_entry *head = as->page_table;
		while(head != NULL){
			if(head->vpn >= as->heap_start + as->heap_pages * PAGE_SIZE && head->vpn < as->heap_start + as->heap_pages * PAGE_SIZE - amount){
				as->page_table = head->next;	
				kfree((void *)PADDR_TO_KVADDR(head->ppn));
				kfree(head);
				head = as->page_table;
			}else{
				break;	
			}
		}
		//if(head == NULL || (head != NULL && head->next == NULL)) return 0;
		if(head == NULL){
			as_activate();
			return 0;	
		} 
		KASSERT(head != NULL);
		struct page_table_entry *next_entry = head->next;
		while(next_entry != NULL){
			if(next_entry->vpn >= as->heap_start + as->heap_pages * PAGE_SIZE && next_entry->vpn < as->heap_start + as->heap_pages * PAGE_SIZE - amount){
				head->next = next_entry->next;
				kfree((void *)PADDR_TO_KVADDR(next_entry->ppn));
				kfree(next_entry);
				next_entry = head->next;
			}else{
				KASSERT(head->next == next_entry);
				head = next_entry;
				next_entry = next_entry->next;	
			}	
		}
		
		as_activate(); // invalid all entries after modifying
	}

	return 0;
}

#endif
