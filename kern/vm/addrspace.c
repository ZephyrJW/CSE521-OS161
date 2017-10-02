/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>


/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */


struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->segments = NULL;
	as->page_table = NULL;
	as->tail = NULL;
	as->heap_start = 0;
	as->heap_pages = 0;
	//as->stack_top = USERSTACK;
	//as->stack_pages = VM_STACKPAGES;

	return as;
}

void
as_destroy(struct addrspace *as)
{
	struct segment *seg_head = as->segments;
	struct segment *tmp1 = NULL;
	while(seg_head != NULL){
		tmp1 = seg_head->next;
		kfree(seg_head);
		seg_head = tmp1;	
	}

	struct page_table_entry *pt_head = as->page_table;
	struct page_table_entry *tmp2 = NULL;
	while(pt_head != NULL){
		tmp2 = pt_head->next;
		kfree((void *)PADDR_TO_KVADDR(pt_head->ppn));
		kfree(pt_head);
		pt_head = tmp2;	
	}
/*
	int count = 0;
	seg_head = as->segments;
	pt_head = as->page_table;
	while(seg_head != NULL){
		count++;
		seg_head = seg_head->next;	
	}
	kprintf("count:%d\n", count);

	count = 0;
	while(pt_head != NULL){
		count++;
		pt_head = pt_head->next;	
	}
	kprintf("count:%d\n", count);
*/
	KASSERT(pt_head == NULL);
	KASSERT(seg_head == NULL);
	KASSERT(tmp1 == NULL);
	KASSERT(tmp2 == NULL);
	as->tail = NULL;
	as->segments = NULL;
	as->page_table = NULL;

	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;


	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write 
	(void)readable;
	(void)writeable;
	(void)executable;
	*/

	struct segment *new = (struct segment *)kmalloc(sizeof(struct segment));
	if(new == NULL){
		return ENOMEM;	
	}

	new->start = vaddr;
	new->seg_pages = npages;
	new->permission = readable | writeable | executable;
	new->next = NULL;

	if(as->segments == NULL){
		as->segments = new;	
	}else{
		struct segment *head = as->segments;
		while(head->next!= NULL) head = head->next;	
		head->next = new;
	}

	KASSERT(as->heap_start < new->start + new->seg_pages * PAGE_SIZE);
	as->heap_start = new->start + new->seg_pages * PAGE_SIZE;	

	return 0;
}

/*
static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}
*/

int
as_prepare_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	(void)as;

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->heap_start = old->heap_start;
	new->heap_pages = old->heap_pages;
	//new->tail = old->tail;
	//as_create() will init these value.
	//new->stack_top = old->stack_top;
	//new->stack_pages = old->stack_pages;

	/*copy region information*/
	struct segment *old_seg = old->segments;
	while(old_seg != NULL){
		struct segment *new_seg = (struct segment *)kmalloc(sizeof(struct segment));
		if(new_seg == NULL){
	//		as_destroy(new);	
			return ENOMEM;	
		}
		new_seg->start = old_seg->start;
		new_seg->seg_pages = old_seg->seg_pages;
		new_seg->permission = old_seg->permission;
		new_seg->next = NULL;
		

		if(new->segments == NULL){
			new->segments = new_seg;
		}else{
			struct segment *tmp1 = new->segments;
			while(tmp1->next != NULL) tmp1 = tmp1->next;	
			KASSERT(tmp1->next == NULL);
			tmp1->next = new_seg;
		}

		old_seg = old_seg->next;
	}

	/*copy page_table*/
	struct page_table_entry *old_pt = old->page_table;
	while(old_pt != NULL){
		struct page_table_entry *new_pt = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
		if(new_pt == NULL){
			//as_destroy(new);
			return ENOMEM;	
		}
		
		new_pt->vpn = old_pt->vpn;
		new_pt->ppn = getppages(1);
		if(new_pt->ppn == 0){
			//kfree(new_pt);
			//as_destroy(new);
			return ENOMEM;
		}
		new_pt->next = NULL;

		//bzero((void *)PADDR_TO_KVADDR(new_pt->ppn), PAGE_SIZE);
		memmove((void *)PADDR_TO_KVADDR(new_pt->ppn),
			(const void *)PADDR_TO_KVADDR(old_pt->ppn),
			PAGE_SIZE);


		if(new->page_table == NULL){
			new->page_table = new_pt;	
			new->tail = new_pt;
		}else{
			/*
			struct page_table_entry *tmp2 = new->page_table;
			while(tmp2->next != NULL) tmp2 = tmp2->next;
			KASSERT(tmp2->next == NULL);
			tmp2->next = new_pt;	
			*/
			new->tail->next = new_pt;
			new->tail = new->tail->next;
		}

		old_pt = old_pt->next;
	}
	//KASSERT(new->tail->next == NULL);

	*ret = new;
	return 0;
}
