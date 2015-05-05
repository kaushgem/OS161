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

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */
vaddr_t as_reset_rw_permission(vaddr_t *vadd);
vaddr_t as_set_rw_permission(vaddr_t *vadd);

int get_permissions_int(int r, int w, int x);

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */

	as->pte = NULL;

	as->as_vbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_npages2 = 0;
	as->hend = 0;
	as->hstart = 0;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	// copy regions
	newas->as_vbase1 = old->as_vbase1;
	newas->as_npages1 = old->as_npages1;
	newas->as_vbase2 = old->as_vbase2;
	newas->as_npages2 = old->as_npages2;


	// copy page table entries


	struct page_table_entry *oldpteHead = old->pte;
	if (oldpteHead != NULL)
	{

		struct page_table_entry *newpteHead = kmalloc(sizeof(struct page_table_entry));
		newpteHead->va = oldpteHead->va;

		// call coremap to get the new virtual address after copy



		newpteHead->pa = alloc_userpage(newas,oldpteHead->va);

		//kprintf("\n copying from %u  to %u ", oldpteHead->va,2 );
		memmove((void *)PADDR_TO_KVADDR(newpteHead->pa),
				(const void *)PADDR_TO_KVADDR(oldpteHead->pa),
				PAGE_SIZE);



		struct page_table_entry *newpte = newpteHead;
		oldpteHead = oldpteHead->next;


		while(oldpteHead != NULL) {
			newpte->next = kmalloc(sizeof(struct page_table_entry));
			newpte->va = oldpteHead->va;
			// newpte->physical_addr =

			newpte->pa = alloc_userpage(newas,oldpteHead->va);
			memmove((void *)PADDR_TO_KVADDR(newpte->pa),
					(const void *)PADDR_TO_KVADDR(oldpteHead->pa),
					PAGE_SIZE);
			//kprintf("\n loop: copying from va %u  pa %u to pa %u ", oldpteHead->va,oldpteHead->pa, newpte->pa);
			//panic("copying page table va: %d old pa: %d new pa: %d",newpte->va,oldpteHead->pa,newpte->pa );


			newpte=newpte->next;
			oldpteHead = oldpteHead->next;
		}
		newpte->next = NULL;
	}
	else
	{
		kprintf("\n something wrong :-O");
	}
	// copy heap limits

	newas->hend = old->hend;
	newas->hstart = old->hstart;





	(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */


	struct page_table_entry *pte = as->pte;
	while(pte!=NULL)
	{
		struct page_table_entry *next = pte->next;
		kfree(pte);
		pte = next;
	}

	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	vm_tlbshootdown_all();
	(void)as;  // suppress warning until code gets written
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;



	int permission = get_permissions_int( readable,  writeable,  executable);


	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr|permission;
		as->as_npages1 = npages;

	}else if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr|permission;
		as->as_npages2 = npages;
	}

	if (as->hstart < (vaddr + sz)) {
		as->hstart = vaddr + sz;
		as->hend = as->hstart;
	}

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	as_set_rw_permission(&as->as_vbase1);
	as_set_rw_permission(&as->as_vbase2);






	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	as_reset_rw_permission(&as->as_vbase1);
	as_reset_rw_permission(&as->as_vbase2);

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

int get_permissions_int(int r, int w, int x)
{

	if(r>0)
		r=1;
	if(w>0)
		w=1;
	if(x >0)
		x=1;


	//kprintf("\n getting permission ints r: %d w :%d x :%d",r,w,x);

	int op = 0;
	op=r+(w<<w)+(x<<2*x);

	//kprintf("\n permission int : %d",op);
	return op;
}


vaddr_t as_set_rw_permission(vaddr_t *vadd)
{

	int l3=(*vadd)&7;
	int  b3=7;
	int  lb=(l3<<3)|b3;
	*vadd = (*vadd)|lb;
	return *vadd;

}

vaddr_t as_reset_rw_permission(vaddr_t *vadd)
{
	int lb3=(*vadd)&0x3F;
	lb3 = lb3>>3;
	*vadd = (*vadd)&~0x3F;
	*vadd=(*vadd)|lb3;
	return *vadd;

}


