/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

/*
 * Use these stubs to test your reader-writer locks.
 */


//we start test console test here in rwt2
//#include <stdlib.h>
//#include <stdio.h>
//#include <errno.h>
//#include <err.h>




static struct rwlock* rwl;
static struct semaphore* testsem;

#define NUM 20

static
void
reader_wrapper(void * unused1, unsigned long index) {
	(void)unused1;
	(void)index;
	rwlock_acquire_read(rwl);
	kprintf_n("start, reader\n");
	for (int i = 0; i < 1000; ++i)
	{
		for (int j = 0; j < 10000; ++j) { }
	}
	rwlock_release_read(rwl);
	kprintf_n("done, reader\n");
	V(testsem);
	return;
}

static
void
writer_wrapper(void * unused1, unsigned long index) {
	(void)unused1;
	(void)index;
	rwlock_acquire_write(rwl);
	kprintf_n("start, writer\n");
	for (int i = 0; i < 10000; ++i)
	{
		for (int j = 0; j < 10000; ++j) { }
	}
	rwlock_release_write(rwl);
	kprintf_n("done, writer\n");
	V(testsem);
	return;
}

int rwtest(int nargs, char **args) {

	(void)nargs;
	(void)args;
	
	int err = 0;
	rwl = rwlock_create("rwl");
	testsem = sem_create("testsem", 0);
	for (int i = 0; i < NUM; ++i)
	{
		err = thread_fork("test", NULL, reader_wrapper, NULL, i);
	}
	for (int i = 0; i< 5; ++i)
	{
	err = thread_fork("test", NULL, writer_wrapper, NULL, i + 20);
	}
	for (int i = 0; i < NUM; ++i)
	{
		err = thread_fork("test", NULL, reader_wrapper, NULL, 30 + i);
	}
	for (int i = 0; i < NUM; ++i)
	{
		err = thread_fork("test", NULL, writer_wrapper, NULL, 50 + i);
	}	

	
	kprintf_n("rwt1 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt1");
	
	for (int i = 0; i < 50; ++i)
	{
		P(testsem);
	}
	kprintf_n("done!\n");
	
	if (err) 
	{
		panic("sp1: thread_fork failed: (%s)\n", strerror(err));
	}

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

//	const char* a = "Hello world!\n";
//	int x = write(1, a, 13);

	kprintf_n("rwt2 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt2");

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt3 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt4 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt5 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}
