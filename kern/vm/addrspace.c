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

	as->hend = NULL;
	as->hstart = NULL;
	as->pte = NULL;
	as->reg = NULL;

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

	struct region *oldHead = old->reg;
	if (oldHead == NULL) return NULL;

	struct region *newHead = kmalloc(sizeof(struct region));
	newHead->readable = oldHead->readable;
	newHead->writeable = oldHead->writeable;
	newHead->executable = oldHead->executable;
	newHead->size = oldHead->size;
	newHead->va = oldHead->va;

	struct region *newreg = newHead;
	oldHead = oldHead->next;


	while(oldHead != NULL) {
		newreg->next = malloc(sizeof(struct region));

		newreg->readable = oldHead->readable;
		newreg->writeable = oldHead->writeable;
		newreg->executable = oldHead->executable;
		newreg->size = oldHead->size;
		newreg->va = oldHead->va;

		newreg=newreg->next;
		oldHead = oldHead->next;
	}
	newreg->next = NULL;





	// copy page table entries


	struct page_table_entry *oldpteHead = old->pte;
		if (oldpteHead == NULL) return NULL;

		struct page_table_entry *newpteHead = kmalloc(sizeof(struct page_table_entry));
		newpteHead->core_index= oldpteHead->core_index;
		newpteHead->permissions = oldpteHead->permissions;
		newpteHead->va = oldpteHead->va;

		// call coremap to get the new virtual address after copy

	//	newpteHead->physical_addr =

		//

		struct page_table_entry *newpte = newpteHead;
		oldpteHead = oldpteHead->next;


		while(oldpteHead != NULL) {
			newpte->next = malloc(sizeof(struct page_table_entry));

			newpte->core_index = oldpteHead->core_index;
			newpte->permissions = oldpteHead->permissions;
			newpte->va = oldpteHead->va;
			// newpte->physical_addr =

			newpte=newpte->next;
			oldpteHead = oldpteHead->next;
		}
		newpte->next = NULL;

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

	// align vaddr
	if(vaddr&~(vaddr_t)PAGE_FRAME !=0)
	{
		vaddr = PAGE_SIZE + (vaddr/PAGE_SIZE )* PAGE_SIZE;
	}

	// align size
	if(sz&~(vaddr_t)PAGE_FRAME !=0)
	{
		sz = PAGE_SIZE + (sz/PAGE_SIZE ) * PAGE_SIZE;
	}



	struct region *headreg = as->reg;

	while(headreg!=NULL)
	{
		headreg = headreg->next;
	}


	struct region *newreg= malloc(sizeof(struct region));
	if(newreg == NULL)
	{
		return ENOMEM;
	}

	newreg->readable = readable;
	newreg->writeable = writeable;
	newreg->executable = executable;
	newreg->size = sz;
	newreg->va = vaddr;

	newreg->next = NULL;
	headreg ->next = newreg;

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

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

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

