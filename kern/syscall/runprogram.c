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

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <file_syscalls.h>
#include <process_syscalls.h>
#include <copyinout.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char *argv[], int argc)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;


	// Copying arguments into Kernel space

	char *kernel_buffer[argc];
	size_t size[argc];
	size_t size_with_padding[argc];

	if(argc > 0)
	{
		for(int i=0 ; i < argc ; i++){
			size[i] = strlen(argv[i])+1;
			kernel_buffer[i] = kmalloc( sizeof(int32_t) * size[i] );
			copyin((const_userptr_t) argv[i], (void*) kernel_buffer[i], size[i]);
			size_with_padding[i] = size[i] + (4 - size[i]%4) /* padding */;
		}
	}



	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	KASSERT(curthread->t_addrspace == NULL);

	/* Create a new address space. */
	curthread->t_addrspace = as_create();
	if (curthread->t_addrspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}


	/* Activate it. */
	as_activate(curthread->t_addrspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		return result;
	}


	//stdin,out,err

	char *name = NULL;

	name = kstrdup("con:");
	curthread->t_fdtable[0] = create_fhandle(name);
	vfs_open(name, O_RDONLY, 0664, &(curthread->t_fdtable[0]->vn));
	kfree(name);

	name = kstrdup("con:");
	curthread->t_fdtable[1] = create_fhandle(name);
	vfs_open(name, O_WRONLY, 0664, &(curthread->t_fdtable[1]->vn));
	kfree(name);

	name = kstrdup("con:");
	curthread->t_fdtable[2] = create_fhandle(name);
	vfs_open(name, O_WRONLY, 0664, &(curthread->t_fdtable[2]->vn));
	kfree(name);
	//



	// Loading user stack from kernel buffer

	int i;
	unsigned int stackptr_for_argv[argc+1];
	size_t len_from_top = 0;

	if(argc > 0)
	{
		for(i=0 ; i < argc ; i++){
			len_from_top = len_from_top + size_with_padding[i];
			stackptr_for_argv[i] = stackptr - len_from_top;
			copyout(kernel_buffer[i], (void*) stackptr_for_argv[i], size_with_padding[i]*sizeof(int32_t));
		}
		stackptr_for_argv[i] = 0; // Last element to identify end of arg
		size_t size_of_argv_ptrs = sizeof(unsigned int)*(argc+1);
		size_t total_len = len_from_top + size_of_argv_ptrs;
		userptr_t user_args_stackptr = (userptr_t) stackptr - total_len;

		copyout(stackptr_for_argv, user_args_stackptr, size_of_argv_ptrs);

		/* Warp to user mode. */
		enter_new_process( argc /*argc*/, user_args_stackptr /*userspace addr of argv*/,
				stackptr, entrypoint);
	}
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

