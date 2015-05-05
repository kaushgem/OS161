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

int as_get_permission(vaddr_t vadd);
int validate_permission(int faulttype, vaddr_t faultaddr);
paddr_t get_physical_address(int code_index);

void vm_bootstrap(void){
	paddr_t start, end, free_addr;
	ram_getsize(&start, &end);
	//total_pages = ((end - start) - ((end -start)%4)) / PAGE_SIZE;
	//total_pages = (end - start) / PAGE_SIZE;
	total_pages = ROUNDUP(end, PAGE_SIZE) / PAGE_SIZE;
	coremap = (struct coremap_entry*) PADDR_TO_KVADDR(start);
	free_addr = start + total_pages * sizeof(struct coremap_entry);

	kprintf("\n*****************************");
	kprintf("\nstart %d end %d",start,end);
	kprintf("\nfree %d totpages %d pagesize %d",free_addr,total_pages, PAGE_SIZE);
	kprintf("\nround %d\n",ROUNDUP(end, PAGE_SIZE));

	coremap_base = start;
	paddr_t addr;

	//for(i=0, addr = start; i < total_pages; i++, addr+=PAGE_SIZE){
	for(int i=0; i< total_pages ; i++){
		coremap[i].vaddr = 0;
		coremap[i].as = NULL;
		coremap[i].npages = 0;

		addr = coremap_base + i * PAGE_SIZE;
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

			addr = coremap_base + i * PAGE_SIZE;

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
		coremap[i].vaddr = 0;
		coremap[i].npages = 1;
		coremap[i].state = FREE;
		i++;
	}

	spinlock_release(&coremap_lock);
}


paddr_t alloc_userpage(struct addrspace *as, vaddr_t vaddr){
	paddr_t addr;

	spinlock_acquire(&coremap_lock);

	for(int i=0; i<total_pages; i++){
		if(coremap[i].state == FREE){
			addr = coremap_base + i * PAGE_SIZE;
			coremap[i].vaddr = vaddr;
			coremap[i].as = as;
			coremap[i].npages = 1;
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
				coremap[i].npages = 0;
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


int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

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

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - VM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	int error = 0;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		error = validate_permission(faulttype, faultaddress);
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		error = validate_permission(faulttype, faultaddress);
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		error = validate_permission(faulttype, faultaddress);
	}
	else if (faultaddress >= as->hstart && faultaddress < as->hend) {
		error = validate_permission(faulttype, faultaddress);
	}
	else {
		return EFAULT;
	}
	if(error >0)
	{
		return error;
	}

	// page is in pte

	struct page_table_entry *ptehead = as->pte;


	while(ptehead!=NULL)
	{
		if(faultaddress >= ptehead->va && faultaddress <(ptehead->va+PAGE_SIZE))
		{

			paddr = (faultaddress - ptehead->va) + ptehead->pa;

		}
		ptehead = ptehead->next;
	}




	// page is not in pte;

	paddr = alloc_userpage(as,faultaddress) ;
	struct page_table_entry *newpte = kmalloc(sizeof(struct page_table_entry));
	newpte->pa = paddr;
	newpte->va = faultaddress;
	newpte->next = NULL;
	ptehead->next = newpte;

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

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

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
}


paddr_t get_physical_address(int code_index)
{
	return coremap_base + code_index*PAGE_SIZE;

}


int validate_permission(int faulttype, vaddr_t faultaddr)
{

	int isReadable = faultaddr&1;
	int isWriteable = faultaddr&2;


	switch (faulttype)
	{
	case VM_FAULT_WRITE:
	{
		if(!isWriteable)
		{
			return EINVAL;
		}
		break;
	}
	case VM_FAULT_READ:
	{
		if(!isReadable)
		{
			return EINVAL;
		}
		break;
	}

	case VM_FAULT_READONLY:
	{
		if(!isWriteable)
		{
			return EINVAL;
		}
		break;
	}

	}
	return 0;


}

int as_get_permission(vaddr_t vadd)
{
	int op = 0;
	op=vadd&7;
	return op;
}

