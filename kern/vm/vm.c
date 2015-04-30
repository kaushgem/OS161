#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;

int total_pages = 0;
bool is_vm_bootstrapped = false;
paddr_t coremap_base = 0;

void vm_bootstrap(void){
	paddr_t start, end, free_addr;
	ram_getsize(&start, &end);
	total_pages = (end - start) / PAGE_SIZE;
	//int total_pages = ROUNDDOWN(end, PAGE_SIZE) / PAGE_SIZE;
	coremap = (struct coremap_entry*) PADDR_TO_KVADDR(start);
	free_addr = start + total_pages * sizeof(struct coremap_entry);

	coremap_base = start;
	paddr_t addr;
	int i;

	//for(int i=0; i< total_pages ; i++){
	for(i=0, addr = start; i < total_pages; i++, addr+=PAGE_SIZE){

		coremap[i].vaddr = 0;
		coremap[i].as = NULL;
		coremap[i].npages = 1;

		if(addr < free_addr)
			coremap[i].state = FIXED;
		else
			coremap[i].state = FREE;
	}

	is_vm_bootstrapped = true;
}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages){
	paddr_t pa;

	if(!is_vm_bootstrapped){
		pa = getppages(npages);
	}else{
		pa = getppages_vm(NULL, npages);
	}

	if (pa==0) {
		return 0;
	}

	return PADDR_TO_KVADDR(pa);
}

vaddr_t alloc_upages(struct addrspace *as, int npages){
	paddr_t pa;

	if(!is_vm_bootstrapped){
		pa = getppages(npages);
	}else{
		pa = getppages_vm(as, npages);
	}

	if (pa==0) {
		return 0;
	}

	return PADDR_TO_KVADDR(pa);
}

paddr_t getppages(int npages){
	paddr_t addr;
	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}

paddr_t getppages_vm(struct addrspace *as, int npages){
	paddr_t addr;
	bool found_pages = true;

	spinlock_acquire(&coremap_lock);

	for(int i=0; i<total_pages; i++){
		found_pages = true;

		for(int j=0; j<npages; j++){
			if(coremap[i+j].state != FREE){
				found_pages = false;
				i = i+j;
				break;
			}
		}

		if(found_pages){

			addr = coremap_base + 4 * i; //4k padding

			coremap[i].vaddr = PADDR_TO_KVADDR(addr);
			coremap[i].npages = npages;

			if(as == NULL){				// Kernel space
				for(int k=0; k<npages ; k++){
					coremap[i++].state = FIXED;
				}
			}else{						// User space
				coremap[i].as = as;
				for(int k=0; k<npages ; k++){
					coremap[i++].state = DIRTY;
				}
			}
			break;
		}
	}

	spinlock_release(&coremap_lock);
	return addr;
}

void free_kpages(vaddr_t addr){

	int npages_to_free = 0;
	spinlock_acquire(&coremap_lock);

	int i;
	for(i=0 ; i<total_pages ; i++){
		if(addr == coremap[i].vaddr)
			npages_to_free = coremap[i].npages;
	}

	for(int j=0 ; j<npages_to_free ; j++){
		coremap[i].npages = 1;
		coremap[i].state = FREE;
		coremap[i].vaddr = 0;
		i++;
	}

	spinlock_release(&coremap_lock);
}

void free_upages(vaddr_t addr){

	int npages_to_free = 0;
	spinlock_acquire(&coremap_lock);

	int i;
	for(i=0 ; i<total_pages ; i++){
		if(addr == coremap[i].vaddr){
			if(coremap[i].state == FIXED){
				kprintf("Cannot free, It's a kernel page");
				return;
			}else{
				npages_to_free = coremap[i].npages;
			}
		}
	}

	for(int j=0 ; j<npages_to_free ; j++){
		coremap[i].npages = 1;
		coremap[i].state = FREE;
		coremap[i].vaddr = 0;
		i++;
	}

	spinlock_release(&coremap_lock);
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown_all(void){

}
void vm_tlbshootdown(const struct tlbshootdown * tlb){
	(void)tlb;
}


/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress){
	(void)faulttype;
	(void)faultaddress;
	return 0;
}
