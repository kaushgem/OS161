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
static int vm_faultcounter = 0;

int total_pages = 0;
bool is_vm_bootstrapped = false;
paddr_t coremap_base = 0;

int as_get_permission(vaddr_t vadd);
int validate_permission(int faulttype, int faultaddr);
paddr_t get_physical_address(int code_index);
int bp(void);

void vm_bootstrap(void){

	paddr_t start, end, free_addr;

	ram_getsize(&start, &end);

	start = ROUNDUP(start, PAGE_SIZE);
	end = (end / PAGE_SIZE)*PAGE_SIZE;

	coremap = (struct coremap_entry*) PADDR_TO_KVADDR(start);

	free_addr = start + total_pages * sizeof(struct coremap_entry);
	free_addr = ROUNDUP(free_addr, PAGE_SIZE);

	total_pages = (end - free_addr) / PAGE_SIZE;

	kprintf("\n************RAM Address*****************");
	kprintf("\nStart:%d End:%d End(ROUNDUP):%d",start,end,ROUNDUP(end, PAGE_SIZE));
	kprintf("\nFree:%d TotalPages:%d\n\n",free_addr,total_pages);

	coremap_base = free_addr;
	paddr_t addr;

	spinlock_acquire(&coremap_lock);

	for(int i=0; i< total_pages ; i++){
		addr = coremap_base + i * PAGE_SIZE;

		coremap[i].vaddr = 0;
		coremap[i].as = NULL;
		coremap[i].npages = 1;
		coremap[i].state = FREE;
	}

	spinlock_release(&coremap_lock);

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

			addr = coremap_base + i * PAGE_SIZE;

			coremap[i].vaddr = PADDR_TO_KVADDR(addr);
			coremap[i].npages = npages;
			for(int k=0; k<npages ; k++){
				//bzero((void *)PADDR_TO_KVADDR(addr), PAGE_SIZE);
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
		coremap[i].vaddr = 0;
		coremap[i].npages = 1;
		coremap[i].state = FREE;

		paddr_t addr = coremap_base + i * PAGE_SIZE;
		bzero((void *)PADDR_TO_KVADDR(addr),PAGE_SIZE);

		i++;
	}

	spinlock_release(&coremap_lock);
}


paddr_t alloc_userpage(struct addrspace *as, vaddr_t vaddr){
	paddr_t addr = 0;

	spinlock_acquire(&coremap_lock);

	for(int i=0; i<total_pages; i++){
		if(coremap[i].state == FREE){
			addr = coremap_base + i * PAGE_SIZE;
			coremap[i].vaddr = vaddr;
			coremap[i].as = as;
			coremap[i].npages = 1;
			coremap[i].state = DIRTY;
			//bzero((void *)PADDR_TO_KVADDR(addr),PAGE_SIZE);

			break;
		}
	}

	spinlock_release(&coremap_lock);

	KASSERT(addr != 0);

	return addr;
}


void free_userpage(vaddr_t vaddr){

	bool isFreed = false;

	spinlock_acquire(&coremap_lock);

	int i;
	for(i=0 ; i<total_pages ; i++){
		if(vaddr == coremap[i].vaddr){
			if(coremap[i].state == FIXED){
				kprintf("\n Err** Cannot free (%d), It's a kernel page\n",vaddr);
				spinlock_release(&coremap_lock);
				return;
			}else{
				coremap[i].vaddr = 0;
				coremap[i].as = NULL;
				coremap[i].npages = 0;
				coremap[i].state = FREE;
				isFreed = true;

				paddr_t addr = coremap_base + i * PAGE_SIZE;
				bzero((void *)PADDR_TO_KVADDR(addr),PAGE_SIZE);

				break;
			}
		}
	}

	spinlock_release(&coremap_lock);

	//KASSERT(isFreed == true);
	//if(!isFreed)
	//kprintf("\n Err** Virtual (%d) not found\n",vaddr);
}


void vm_tlbshootdown_all(void){
	int i, spl;
	spl = splhigh();
	for (i=0; i<NUM_TLB; i++)
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	splx(spl);
}


void vm_tlbshootdown(const struct tlbshootdown * tlb){
	(void)tlb;
	int i, spl;
	spl = splhigh();
	for (i=0; i<NUM_TLB; i++)
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	splx(spl);
}


int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	uint32_t ehi, elo;

	struct addrspace *as;
	int i, spl;

	vm_faultcounter += 1;
	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	as = curthread->t_addrspace;
	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_npages2 != 0);

	(void)faulttype;
	vbase1 = as->as_vbase1;
	int vbase1perm = vbase1&~PAGE_FRAME;
	vbase1 = vbase1&PAGE_FRAME;

	vbase2 = as->as_vbase2;
	int vbase2perm = vbase2&~PAGE_FRAME;
	vbase2 = vbase2&PAGE_FRAME;

	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;

	stackbase = USERSTACK - VM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	//KASSERT((faultaddress & PAGE_FRAME) ==faultaddress);

	if(faultaddress == 4210688){
		bp();
	}

	int error = 0;

	if (faultaddress >= vbase1 && faultaddress <= vtop1) {
		error = validate_permission(faulttype, vbase1perm);
	}else if (faultaddress >= vbase2 && faultaddress <= vtop2) {
		error = validate_permission(faulttype, vbase2perm);
	}else if (faultaddress >= stackbase && faultaddress <= stacktop) {
		error = validate_permission(faulttype, 7);
	}else if (faultaddress >= as->hstart && faultaddress <= as->hend) {
		error = validate_permission(faulttype, 7);
	}else {
		panic("\n**Err**Not in any region || Faultaddress: %d \n\n",faultaddress);
		//return EFAULT;
	}

	if(error > 0){
		return error;
	}

	// page is in pte
	bool ispageInPte = false;
	struct page_table_entry *ptehead = as->pte;
	struct page_table_entry *prevpte = ptehead;

	while(ptehead!=NULL ){
		if(faultaddress >= ptehead->va && faultaddress < (ptehead->va + PAGE_SIZE)){
			paddr = (faultaddress - ptehead->va) + ptehead->pa;
			ispageInPte = true;
			break;
		}
		prevpte = ptehead;
		ptehead = ptehead->next;
	}

	if(!ispageInPte){

		struct page_table_entry *newpte = kmalloc(sizeof(struct page_table_entry));
		newpte->va = faultaddress;

		paddr = alloc_userpage(as,faultaddress);

		newpte->pa = paddr;
		newpte->next = NULL;

		if(as->pte==NULL){
			as->pte = newpte;
		}else{
			prevpte->next = newpte;
		}
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	/*for (i=0; i<NUM_TLB; i++)
	{
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
	}*/

	i = tlb_probe((uint32_t)faultaddress, (uint32_t)paddr);
	if(i!=-1){
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}else{
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_random(ehi, elo);
		splx(spl);
		return 0;
	}

	// Jinghao comments

	splx(spl);
	return EFAULT;
}




// Utility Functions


int bp()
{
	return 0;
}


paddr_t get_physical_address(int code_index)
{
	return coremap_base + code_index*PAGE_SIZE;
}


int validate_permission(int faulttype, int  permission)
{
	//int isReadable = permission&1;
	int isWriteable = permission&2;

	switch (faulttype)
	{
	case VM_FAULT_WRITE:
		if(!isWriteable)
			return EINVAL;
		break;
	case VM_FAULT_READONLY:
		if(!isWriteable)
			return EINVAL;
		break;
	}
	return 0;
}


int as_get_permission(vaddr_t vadd)
{
	int op = 0;
	op=vadd&7;
	return op;
}

