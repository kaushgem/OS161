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


vaddr_t alloc_kpages(int npages){
	paddr_t pa;

	if(!is_vm_bootstrapped){
		pa = getppages(npages);
	}else{
		pa = getppages_vm(npages);
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


paddr_t getppages_vm(int npages){
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

			addr = coremap_base + i * PAGE_SIZE; //4k padding

			coremap[i].vaddr = PADDR_TO_KVADDR(addr);
			coremap[i].npages = npages;
			for(int k=0; k<npages ; k++){
				coremap[i++].state = FIXED;
			}

			break;
		}
	}

	spinlock_release(&coremap_lock);
	return addr;
}


void free_kpages(vaddr_t vaddr){

	int npages_to_free = 0;
	spinlock_acquire(&coremap_lock);

	int i;
	for(i=0 ; i<total_pages ; i++){
		if(vaddr == coremap[i].vaddr){
			npages_to_free = coremap[i].npages;
			break;
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


paddr_t alloc_userpage(struct addrspace *as, vaddr_t vaddr){
	paddr_t addr;

	spinlock_acquire(&coremap_lock);

	for(int i=0; i<total_pages; i++){
		if(coremap[i].state == FREE){
			//addr = coremap_base + i * PAGE_SIZE; //4k padding
			//coremap[i].vaddr = PADDR_TO_KVADDR(addr);
			coremap[i].vaddr = vaddr;
			coremap[i].as = as;
			coremap[i].state = DIRTY;
			break;
		}
	}

	spinlock_release(&coremap_lock);
	return addr;
}


void free_userpage(vaddr_t vaddr){

	spinlock_acquire(&coremap_lock);

	int i;
	for(i=0 ; i<total_pages ; i++){
		if(vaddr == coremap[i].vaddr){
			if(coremap[i].state == FIXED){
				kprintf("\n Err** Cannot free, It's a kernel page\n");
				return;
			}else{
				coremap[i].vaddr = 0;
				coremap[i].as = NULL;
				coremap[i].npages = 1;
				coremap[i].state = FREE;
			}
		}else{
			kprintf("\n Err** Virtual Page number not found\n");
			return;
		}
	}

	spinlock_release(&coremap_lock);
}


void vm_tlbshootdown_all(void){
	int i, spl;
	spl = splhigh();
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl);
}


void vm_tlbshootdown(const struct tlbshootdown * tlb){
	(void)tlb;
	int i, spl;
	spl = splhigh();
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl);
}


int vm_fault(int faulttype, vaddr_t faultaddress){
	(void)faulttype;
	(void)faultaddress;



	return 0;
}
