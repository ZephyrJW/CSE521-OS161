/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */
struct lock* lk0;//0
struct lock* lk1;//1
struct lock* lk2;//2
struct lock* lk3;//3
struct lock* lkA; //lcok the locks

void
lk_a(char);

void
lk_r(char);

void
lk_a(char x)
{
	struct lock* name = NULL;
	switch(x) {
		case '0':name = lk0;break;
		case '1':name = lk1;break;
		case '2':name = lk2;break;
		case '3':name = lk3;break;
		case 'A':name = lkA;break;
		default:
			kprintf_n("char error!\n");
	}
	lock_acquire(name);
}

void
lk_r(char x)
{
	struct lock* name = NULL;
	switch(x) {
		case '0':name = lk0;break;
		case '1':name = lk1;break;
		case '2':name = lk2;break;
		case '3':name = lk3;break;
		case 'A':name = lkA;
	}
	lock_release(name);
}

void
turn_left(uint32_t*, uint32_t);


void
stoplight_init() {
	lk0 = lock_create("lock0");
	lk1 = lock_create("lock1");
	lk2 = lock_create("lock2");
	lk3 = lock_create("lock3");
	lkA = lock_create("lockA");
	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	lock_destroy(lk1);
	lock_destroy(lk2);
	lock_destroy(lk0);
	lock_destroy(lk3);
	lock_destroy(lkA);
	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	lk_a('A');
	char x = direction + '0';
	lk_a(x);
	lk_r('A');
	inQuadrant(direction, index);
	leaveIntersection(index);
	lk_r(x);
	
	/*
	 * Implement this function.
	 */
	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	lk_a('A');
	char x = direction + '0';
	char y = (direction + 3) % 4 + '0';
	
	lk_a(x);
	lk_a(y);
	lk_r('A');

	(void)direction;
	(void)index;
	inQuadrant(direction, index);
	turn_left(&direction, index);
	leaveIntersection(index);
	lk_r(x);
	lk_r(y);	
	/*
	 * Implement this function.
	 */
	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	lk_a('A');
	char x = direction + '0';
	char y = (direction + 3)% 4 + '0';
	char z = (direction + 2)% 4 + '0';
	lk_a(x);
	lk_a(y);
	lk_a(z);
	lk_r('A');
	(void)direction;
	(void)index;
	inQuadrant(direction, index);
	turn_left(&direction, index);
	turn_left(&direction, index);
	leaveIntersection(index);
	lk_r(x);
	lk_r(y);
	lk_r(z);
	/*
	 * Implement this function.
	 */
	return;
}

void
turn_left(uint32_t* direction, uint32_t index)
{
	if (*direction == 0) *direction = 3;
	else (*direction)--;
	inQuadrant(*direction, index);
}
