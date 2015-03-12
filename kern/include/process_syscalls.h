/*
 * process_syscalls.h
 *
 *  Created on: Mar 8, 2015
 *      Author: trinity
 */

#ifndef PROCESS_SYSCALLS_H_
#define PROCESS_SYSCALLS_H_

#include <kern/limits.h>
#include <types.h>
#include <synch.h>
#include <mips/trapframe.h>

struct process_block* pid_array[__PID_MAX];
struct lock* pid_array_lock;
struct spinlock pid_array_spinlock;

struct process_block
{
	pid_t parent_pid;
	//struct lock *process_cv_lock;
	bool exited;
	int exitcode;
	bool childpid[__PID_MAX];
	// struct thread *t;
	//struct child *child;
	struct semaphore *process_sem;
};

struct child
{
	pid_t pid;
	struct child *next;
};

pid_t allocate_processid(void);
struct process_block  *init_process_block(pid_t parenpid);
void destroy_process_block(struct process_block* process);
void destroy_childlist(struct child* childlist);
void add_child(struct child* childlist, pid_t child_pid);
void remove_child(struct child* childlist, pid_t child_pid);
struct addrspace *copy_parent_addrspace(struct addrspace *padrs);
struct trapframe *copy_parent_trapframe(struct  trapframe *ptf);
void child_fork_entry(void *data1, unsigned long data2);

pid_t getpid(void);
pid_t waitpid(pid_t pid, int *status, int options, int *error);
void _exit(int exitcode);
pid_t fork(struct  trapframe* , int *error);
//int execv(const char *program, char **args);

#endif /* PROCESS_SYSCALLS_H_ */
