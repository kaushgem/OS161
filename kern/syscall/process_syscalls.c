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

pid_t allocate_processid(void);
struct process_block  *init_process_block(pid_t parenpid);
void destroy_process_block(struct process_block* process);
void destroy_childlist(struct child* childlist);
void add_child(struct child* childlist, pid_t child_pid);
void remove_child(struct child* childlist, pid_t child_pid);
struct addrspace *copy_parent_addrspace(struct addrspace *padrs);
struct trapframe *copy_parent_trapframe(struct  trapframe *ptf);
void child_fork_entry(void *data1, unsigned long data2);

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

struct process_block  *init_process_block(pid_t parentpid)
{
	struct process_block *pb;
	pb = (struct process_block*) kmalloc(sizeof(struct process_block));
	pb->parent_pid = parentpid;
	pb->process_cv=cv_create("processcv");
	pb->process_cv_lock = lock_create("processlock");
	pb->exited = false;
	pb->t = NULL;
	pb->exitcode = 0;
	pb->child = NULL;
	return NULL;
}

void destroy_process_block(struct process_block* process){
	cv_destroy(process->process_cv);
	lock_destroy(process->process_cv_lock);
	destroy_childlist(process->child);
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

	struct child* head = childlist;

	while(head){
		head = head->next;
	}

	struct  child *childnode;
	childnode = (struct child*) kmalloc(sizeof(struct child));
	childnode->pid = child_pid;
	childnode->next = NULL;
	head->next = childnode;
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


pid_t waitpid(pid_t pid, int *status, int options, int *error)
{
	if(pid < 0 || pid > __PID_MAX || status == NULL){
		*error = EFAULT;
		return -1;
	}

	if(options != 0){
		*error = EINVAL;
		return -1;
	}

	struct process_block *currentProcess = pid_array[getpid()];
	struct process_block *childProcess = pid_array[pid];

	if(childProcess == NULL){
		*error = ESRCH;
		return -1;
	}

	// Check whether its my child
	bool isChild = false;
	while(currentProcess->child){
		if(currentProcess->child->pid == pid){
			isChild = true;
			break;
		}
		currentProcess->child = currentProcess->child->next;
	}

	if(!isChild){
		*error = ECHILD;
		return -1;
	}

	lock_acquire(childProcess->process_cv_lock); // dummy lock
	if(!childProcess->exited){
		cv_wait(childProcess->process_cv,childProcess->process_cv_lock);
	}

	*status = childProcess->exitcode;

	remove_child(currentProcess->child, pid);
	destroy_process_block(childProcess);

	lock_acquire(pid_array_lock);
	pid_array[pid] = NULL;
	lock_release(pid_array_lock);

	return pid;
}

void _exit(int exitcode){

	struct process_block *currentProcess = pid_array[getpid()];
	currentProcess->exited = true;
	currentProcess->exitcode = _MKWAIT_EXIT(exitcode);
	cv_broadcast(currentProcess->process_cv,currentProcess->process_cv_lock);

}

pid_t getpid()
{
	return curthread->pid;
}

pid_t fork(struct  trapframe *ptf, int *error)
{
	struct trapframe *ctf = copy_parent_trapframe(ptf);
	struct addrspace *cadrs = copy_parent_addrspace(curthread->t_addrspace);

	struct thread *childtthread;

	// initialize processs block
	struct process_block  *cpb = init_process_block(getpid());

	// allocate process id
	pid_t cpid = allocate_processid();
	if(cpid==-1)
	{
		*error =ENPROC;
		return -1;
	}


	// assign it to global static array
	lock_acquire(pid_array_lock);
	pid_array[cpid] = cpb;
	lock_release(pid_array_lock);

	// fork the thread
	thread_fork2("fork",
			child_fork_entry,
			(void*)ctf,
			(unsigned long)cadrs,
			&childtthread);

	childtthread->pid = cpid;

	// make child thread runnable
	thread_make_runnable2(childtthread, false);

	return cpid;
}

struct trapframe *copy_parent_trapframe(struct  trapframe *ptf)
{

	struct trapframe *ctf;
	ctf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
	ctf->tf_vaddr= (uint32_t) ptf->tf_vaddr;	/* coprocessor 0 vaddr register */
	ctf->tf_status= (uint32_t)  ptf->tf_status;	/* coprocessor 0 status register */
	ctf->tf_cause= (uint32_t)  ptf->tf_cause;	/* coprocessor 0 cause register */
	ctf->tf_lo= (uint32_t) ptf->tf_lo;
	ctf->tf_hi= (uint32_t) ptf->tf_hi;
	ctf->tf_ra= (uint32_t) ptf->tf_ra	;	/* Saved register 31 */
	ctf->tf_at= (uint32_t) ptf->tf_at	;	/* Saved register 1 (AT) */
	ctf->tf_v0= (uint32_t) ptf->tf_v0	;	/* Saved register 2 (v0) */
	ctf->tf_v1= (uint32_t) ptf->tf_v1	;	/* etc. */
	ctf->tf_a0= (uint32_t) ptf->tf_a0;
	ctf->tf_a1= (uint32_t) ptf->tf_a1;
	ctf->tf_a2= (uint32_t) ptf->tf_a2;
	ctf->tf_a3= (uint32_t) ptf->tf_a3;
	ctf->tf_t0= (uint32_t) ptf->tf_t0;
	ctf->tf_t1= (uint32_t) ptf->tf_t1;
	ctf->tf_t2= (uint32_t) ptf->tf_t2;
	ctf->tf_t3= (uint32_t) ptf->tf_t3;
	ctf->tf_t4= (uint32_t) ptf->tf_t4;
	ctf->tf_t5= (uint32_t) ptf->tf_t5;
	ctf->tf_t6= (uint32_t) ptf->tf_t6;
	ctf->tf_t7= (uint32_t) ptf->tf_t7;
	ctf->tf_s0= (uint32_t) ptf->tf_s0;
	ctf->tf_s1= (uint32_t) ptf->tf_s1;
	ctf->tf_s2= (uint32_t) ptf->tf_s2;
	ctf->tf_s3= (uint32_t) ptf->tf_s3;
	ctf->tf_s4= (uint32_t) ptf->tf_s4;
	ctf->tf_s5= (uint32_t) ptf->tf_s5;
	ctf->tf_s6= (uint32_t) ptf->tf_s6;
	ctf->tf_s7= (uint32_t) ptf->tf_s7;
	ctf->tf_t8= (uint32_t) ptf->tf_t8;
	ctf->tf_t9= (uint32_t) ptf->tf_t9;
	ctf->tf_k0= (uint32_t) ptf->tf_k0;/* dummy (see exception.S comments) */
	ctf->tf_k1= (uint32_t) ptf->tf_k1;		/* dummy */
	ctf->tf_gp= (uint32_t) ptf->tf_gp;
	ctf->tf_sp= (uint32_t) ptf->tf_sp;
	ctf->tf_s8= (uint32_t) ptf->tf_s8;
	ctf->tf_epc= (uint32_t) ptf->tf_epc;

	return ctf;
}

struct addrspace* copy_parent_addrspace(struct addrspace *padrs)
{
	struct addrspace *cadrs;
	as_copy(padrs, &cadrs);
	return cadrs;
}


void child_fork_entry(void *data1, unsigned long data2)
{
	struct trapframe *ctf = (struct trapframe *)data1;
	struct addrspace *cadrs = (struct addrspace *)data2;

	ctf->tf_a0= 0;
	ctf->tf_v0= 0;
	ctf->tf_epc = ctf->tf_epc+4;
	curthread->t_addrspace = cadrs;
	as_activate(curthread->t_addrspace);
	mips_usermode(ctf);
}


//execv




