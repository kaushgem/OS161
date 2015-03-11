/*
 * process_syscalls.c
 *
 *  Created on: Mar 8, 2015
 *      Author: trinity
 */

#include <process_syscalls.h>
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
#include <kern/wait.h>
#include<copyinout.h>

pid_t allocate_processid()
{
	lock_acquire(pid_array_lock);
	for( pid_t i=2; i <__PID_MAX; i++)
	{
		if(pid_array[i] == NULL)
		{
			lock_release(pid_array_lock);
			return i;
		}
	}
	lock_release(pid_array_lock);
	return -1;
}

struct process_block *init_process_block(pid_t parentpid)
{
	struct process_block *pb;
	pb = (struct process_block*) kmalloc(sizeof(struct process_block));
	pb->parent_pid = parentpid;
	// pb->process_cv=cv_create("processcv");
	pb->process_sem = sem_create("proc_sem",0);
	//pb->process_cv_lock = lock_create("processlock");
	pb->exited = false;
	//pb->t = NULL;

	for( int i=0; i < __PID_MAX ; i++)
	{
		pb->childpid[i] = false;
	}

	pb->exitcode = 0;
	// pb->child = NULL;
	return pb;
}

void destroy_process_block(struct process_block* process){

	// cv_destroy(process->process_cv);
	//kprintf("\n destroy_process_block: cv destoryed");
	//lock_destroy(process->process_cv_lock);
	//kprintf("\n destroy_process_block: lock destoryed");
	//destroy_childlist(process->child);
	//kprintf("\n destroy_process_block: childlist destoryed");
	sem_destroy(process->process_sem);
	kfree(process);

}


void destroy_childlist(struct child* childlist){
	struct child* temp_next;

	while(childlist){
		temp_next = childlist->next;
		kfree(childlist);
		childlist = temp_next;
	}
}

void add_child(struct child* childlist, pid_t child_pid){


	//kprintf("\nadd_child:  adding child: %d",(int)child_pid);


	//kprintf("\nadd_child:  creating child node");
	struct  child *childnode;
	childnode = (struct child*) kmalloc(sizeof(struct child));
	childnode->pid = child_pid;
	childnode->next = NULL;
	//kprintf("\nadd_child:  child node created");
	if(childlist==NULL)
	{
		childlist= childnode;
	}
	else
	{
		while(childlist){
			childlist = childlist->next;
		}
		childlist->next = childnode;
	}
	//kprintf("\nadd_child:  child node added to LL");
}

void remove_child(struct child* childlist, pid_t child_pid){

	struct  child* head = childlist;

	struct child* prev;
	prev = head;

	while(head){
		if(head->pid == child_pid){
			prev->next = head->next;
			kfree(head);
			break;
		}
		prev = head;
		head = head->next;
	}
}





pid_t fork(struct trapframe *ptf, int *error)
{
	struct trapframe *ctf;
	ctf = kmalloc(sizeof(struct trapframe));
	memcpy(ctf,ptf, sizeof(struct trapframe));
	struct addrspace *caddr;

	as_copy(curthread->t_addrspace, &caddr);
	struct thread *child_thread;

	*error = thread_fork("fork",
			child_fork_entry,
			(void*)ctf,
			(unsigned long)caddr,
			&child_thread);

	if(*error > 0){
		return -1;
	}


	return child_thread->pid;
}

void child_fork_entry(void *data1, unsigned long data2)
{
	struct trapframe *ctf = (struct trapframe *)data1;
	struct addrspace *caddr = (struct addrspace *)data2;

	ctf->tf_a3= 0;
	ctf->tf_v0= 0;
	ctf->tf_epc = ctf->tf_epc+4;

	curthread->t_addrspace = caddr;
	as_activate(curthread->t_addrspace);

	struct trapframe tf;
	memcpy(&tf,ctf,sizeof(struct trapframe));
	mips_usermode(&tf);
	//KASSERT(SAME_STACK(cpustacks[curcpu->c_number]-1, (vaddr_t)tf_copy));
}

struct trapframe *copy_parent_trapframe(struct  trapframe *ptf)
{
	struct trapframe *ctf;
	ctf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
	ctf->tf_vaddr = (uint32_t) ptf->tf_vaddr;		/* coprocessor 0 vaddr register */
	ctf->tf_status = (uint32_t)  ptf->tf_status;	/* coprocessor 0 status register */
	ctf->tf_cause = (uint32_t)  ptf->tf_cause;		/* coprocessor 0 cause register */
	ctf->tf_lo = (uint32_t) ptf->tf_lo;
	ctf->tf_hi = (uint32_t) ptf->tf_hi;
	ctf->tf_ra = (uint32_t) ptf->tf_ra	;			/* Saved register 31 */
	ctf->tf_at = (uint32_t) ptf->tf_at	;			/* Saved register 1 (AT) */
	ctf->tf_v0 = (uint32_t) ptf->tf_v0	;			/* Saved register 2 (v0) */
	ctf->tf_v1 = (uint32_t) ptf->tf_v1	;			/* etc. */
	ctf->tf_a0 = (uint32_t) ptf->tf_a0;
	ctf->tf_a1 = (uint32_t) ptf->tf_a1;
	ctf->tf_a2 = (uint32_t) ptf->tf_a2;
	ctf->tf_a3 = (uint32_t) ptf->tf_a3;
	ctf->tf_t0 = (uint32_t) ptf->tf_t0;
	ctf->tf_t1 = (uint32_t) ptf->tf_t1;
	ctf->tf_t2 = (uint32_t) ptf->tf_t2;
	ctf->tf_t3 = (uint32_t) ptf->tf_t3;
	ctf->tf_t4 = (uint32_t) ptf->tf_t4;
	ctf->tf_t5 = (uint32_t) ptf->tf_t5;
	ctf->tf_t6 = (uint32_t) ptf->tf_t6;
	ctf->tf_t7 = (uint32_t) ptf->tf_t7;
	ctf->tf_s0 = (uint32_t) ptf->tf_s0;
	ctf->tf_s1 = (uint32_t) ptf->tf_s1;
	ctf->tf_s2 = (uint32_t) ptf->tf_s2;
	ctf->tf_s3 = (uint32_t) ptf->tf_s3;
	ctf->tf_s4 = (uint32_t) ptf->tf_s4;
	ctf->tf_s5 = (uint32_t) ptf->tf_s5;
	ctf->tf_s6 = (uint32_t) ptf->tf_s6;
	ctf->tf_s7 = (uint32_t) ptf->tf_s7;
	ctf->tf_t8 = (uint32_t) ptf->tf_t8;
	ctf->tf_t9 = (uint32_t) ptf->tf_t9;
	ctf->tf_k0 = (uint32_t) ptf->tf_k0;				/* dummy (see exception.S comments) */
	ctf->tf_k1 = (uint32_t) ptf->tf_k1;				/* dummy */
	ctf->tf_gp = (uint32_t) ptf->tf_gp;
	ctf->tf_sp = (uint32_t) ptf->tf_sp;
	ctf->tf_s8 = (uint32_t) ptf->tf_s8;
	ctf->tf_epc = (uint32_t) ptf->tf_epc;

	return ctf;
}

struct addrspace* copy_parent_addrspace(struct addrspace *padrs)
{
	struct addrspace *cadrs;
	as_copy(padrs, &cadrs);
	return cadrs;
}

pid_t getpid()
{
	return curthread->pid;
}

pid_t waitpid(pid_t pid, int *status, int options, int *error)
{

	//kprintf("\nwaitpid: validating  pid: %d" , (int)pid);
	if(pid < 0 || pid > __PID_MAX || status == NULL){
		*error = EFAULT;
		//kprintf("\ninvalid pid");
		return -1;
	}

	//kprintf("\nwaitpid: validating  options");
	if(options != 0){
		*error = EINVAL;
		//kprintf("\ninvalid options");
		return -1;
	}

	//kprintf("\nwaitpid: current process pid:  %d",getpid());

	struct process_block *currentProcess = pid_array[getpid()];
	struct process_block *childProcess = pid_array[pid];

	//kprintf("\nwaitpid: validating  childProcess");
	if(childProcess == NULL){
		*error = ESRCH;
		//kprintf("\ninvalid child process: %d", (int)pid);
		return -1;
	}

	// Check whether its my child
	bool isChild = false;
	//kprintf("\nwaitpid: checking if pid is  child");



	/*while(currentProcess->child){
		kprintf(" \nwaitpid: currentProcess->child: %d pid: %d",(int)currentProcess->child, (int)pid );
		if(currentProcess->child->pid == pid){
			isChild = true;
			break;
		}
		currentProcess->child = currentProcess->child->next;
	}*/

	isChild = currentProcess->childpid[pid];

	if(!isChild){
		//kprintf("\n not a  child process");
		*error = ECHILD;
		return -1;
	}

	//kprintf("\nwaitpid: pid is  child");
	if(!childProcess->exited){
		//kprintf("\nwaitpid: cv waiting");

		P(childProcess->process_sem);
	}
	else
	{
		//kprintf("\n child process alread exited");
	}

	*status = childProcess->exitcode;

	//remove_child(currentProcess->child, pid);

	//kprintf("\nwaitpid: cv waiting done. destroying child process");
	currentProcess->childpid[pid] = false;

	// uncomment it may cause memory leak
	destroy_process_block(childProcess);

	//lock_acquire(pid_array_lock);
	pid_array[pid] = NULL;
	//lock_release(pid_array_lock);

	return pid;
}

void _exit(int exitcode){

	//kprintf("process %d exiting\n" , (int)getpid() );

	struct process_block *currentProcess = pid_array[getpid()];

	if(currentProcess !=NULL)
	{
		currentProcess->exited = true;
		currentProcess->exitcode = _MKWAIT_EXIT(exitcode);
		thread_exit();
	}
	else
	{
		kprintf("curremt process is null. something wrong\n");
	}
}





