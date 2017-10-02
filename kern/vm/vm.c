#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */

/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */

/*
 * Wrap ram_stealmem in a spinlock.
 */
//static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock alloc_lock = SPINLOCK_INITIALIZER;

void
vm_bootstrap(void)
{
	/* Do nothing. */
}


#if 1
//static
paddr_t
getppages(unsigned long npages)
{
	/* dumbvm version
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
	*/
	paddr_t ret = 0;
	unsigned int count = 0;
	spinlock_acquire(&alloc_lock);
	for(int i=first_available_page; i< all_pages; i++){
		if(coremap_ptr[i].status == FREE){
			if(count == 0) ret = i * PAGE_SIZE;
			count++;
			if(count >= npages) break;
		}else{
			if(coremap_ptr[i].status == DIRTY && coremap_ptr[i].pages != 0){
				i += coremap_ptr[i].pages-1;
			}
			count = 0;
		}	
	}

	if(count < npages){
		/*unable to assign npages, not enough available*/
		spinlock_release(&alloc_lock);
		return 0;
	}

	coremap_ptr[ret/PAGE_SIZE].pages = npages;
	for(int i=ret/PAGE_SIZE; count > 0; i++){
		coremap_ptr[i].status = DIRTY;	
		count--;
	}

	spinlock_release(&alloc_lock);
	return ret;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	//dumbvm_can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void
free_kpages(vaddr_t addr)
{
	/* nothing - leak the memory. */
	//(void)addr;
	int index = (addr - MIPS_KSEG0) / PAGE_SIZE;
	/*notice, next line result in quint panic*/
	//KASSERT(coremap_ptr[index].pages != 0);
	int page_cnt = coremap_ptr[index].pages;

	spinlock_acquire(&alloc_lock);
	coremap_ptr[index].pages = 0;
	for(int i=0;i<page_cnt;i++){
		coremap_ptr[i+index].status = FREE;	
	}
	spinlock_release(&alloc_lock);
}

unsigned
int
coremap_used_bytes() {

	/* dumbvm doesn't track page allocations. Return 0 so that khu works. */
	spinlock_acquire(&alloc_lock);
	unsigned int ret = 0;
	for(int i=0; i<all_pages; i++){
		/*can't use coremap[i].pages, since first several fixed pages did not init this value*/
		if(coremap_ptr[i].status != FREE) ret += PAGE_SIZE;
	}

	spinlock_release(&alloc_lock);
	return ret;
}
#endif 

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr;
	//int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "vm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("vm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}
	/*check valid address*/
	bool isValid = false;
	if(faultaddress >= USERSTACK) return EFAULT;
	if(faultaddress < USERSTACK - VM_STACKPAGES * PAGE_SIZE && faultaddress >= as->heap_start + as->heap_pages * PAGE_SIZE) return EFAULT;

	if(faultaddress < as->heap_start){
		struct segment *seg = as->segments;
		while(seg != NULL){
			if(faultaddress >= seg->start && faultaddress < seg->start + seg->seg_pages * PAGE_SIZE){
				isValid = true;
				break;		
			}
			seg = seg->next;
		}	

		if(isValid == false) return EFAULT;
	}
	
	/*page_table*/
	bool found = false;
	struct page_table_entry *pt_head = as->page_table;
//
	while(pt_head != NULL){
		if(faultaddress >= pt_head->vpn && faultaddress < pt_head->vpn + PAGE_SIZE){
			found = true;
			paddr = pt_head->ppn;
			break;	
		}	
		pt_head = pt_head->next;
	}

	pt_head = as->page_table;
	if(found == false){
		struct page_table_entry *new = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry)); 	
		if(new == NULL) return ENOMEM;

		new->vpn = faultaddress;
		paddr_t new_paddr = getppages(1);
		if(new_paddr == 0) {
			kfree(new);
			return ENOMEM;
		}
		new->ppn = new_paddr;	
		//as_zero_region(new_paddr, 1);	
		bzero((void *)(PADDR_TO_KVADDR(new_paddr)), PAGE_SIZE);
		//new->state = UNMAPPED;
		new->next = NULL;
		paddr = new->ppn;

		if(as->page_table == NULL){
			as->page_table = new;	
			as->tail = new;
		}else{
			while(pt_head->next != NULL) pt_head = pt_head->next;
			pt_head->next = new;
			//KASSERT(as->tail->next == NULL);
			//as->tail->next = new;
			//as->tail = as->tail->next;
		}

	}
//
/*
	if(pt_head == NULL){
		struct page_table_entry *new = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry)); 	
		if(new == NULL) return ENOMEM;

		new->vpn = faultaddress;
		paddr_t new_paddr = getppages(1);
		if(new_paddr == 0) {
			kfree(new);
			return ENOMEM;
		}
		new->ppn = new_paddr;	
		//as_zero_region(new_paddr, 1);	
		bzero((void *)(PADDR_TO_KVADDR(new_paddr)), PAGE_SIZE);
		//new->state = UNMAPPED;
		new->next = NULL;
		as->page_table = new;

		paddr = new->ppn;
	}else{
		if(pt_head->next == NULL){
			if(faultaddress >= pt_head->vpn && faultaddress < pt_head->vpn + PAGE_SIZE){
				found = true;
				paddr = pt_head->ppn;
			}
		}

		while(pt_head->next != NULL){
			if(faultaddress >= pt_head->vpn && faultaddress < pt_head->vpn + PAGE_SIZE){
				found = true;
				paddr = pt_head->ppn;
				break;
			}
			pt_head = pt_head->next;

			if(pt_head->next == NULL){
				if(faultaddress >= pt_head->vpn && faultaddress < pt_head->vpn + PAGE_SIZE){
					found = true;
					paddr = pt_head->ppn;	
				}	
			}
		}

		if(found == false){
			struct page_table_entry *new = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
			if(new == NULL) return ENOMEM;
			new->vpn = faultaddress;
			paddr_t new_paddr = getppages(1);
			if(new_paddr == 0) {
				kfree(new);
				return ENOMEM;
			}
			new->ppn = new_paddr;
			//as_zero_region(new_paddr, 1);
			bzero((void *)PADDR_TO_KVADDR(new_paddr), PAGE_SIZE);
			//new->state = UNMAPPED;
			new->next = NULL;
			KASSERT(pt_head->next == NULL);
			pt_head->next = new;	

			paddr = new->ppn;
		}
	}
*/
	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	/*possibly, tlb_random may rewrite existing valid entry, resulting in redundant operation on tlb??*/
	for (int i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID && ehi != faultaddress) {
			continue;
		}

		if(ehi != faultaddress){
			ehi = faultaddress;
		}
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	tlb_random(ehi, elo);
	//kprintf("ehi:%x, %x\n", ehi, faultaddress);
/*
	for(int i = 0; i<NUM_TLB; i++){
		tlb_read(&ehi, &elo, i);
		if(elo & TLBLO_VALID) continue;	
		if(ehi == faultaddress) kprintf("found!\n");
	}


	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}
*/
	splx(spl);
	//kprintf("stacktest\n");
	return 0;
}

