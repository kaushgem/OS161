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

struct process_block
{
	pid_t parent_pid;
	struct cv *process_cv;
	struct lock *process_cv_lock;
	bool exited;
	int exitcode;
	//struct thread* t;
	struct child *child;
};

struct child
{
	pid_t pid;
	struct child *next;
};

pid_t getpid(void);
pid_t waitpid(pid_t pid, int *status, int options, int *error);
void _exit(int exitcode);
pid_t fork(struct  trapframe* , int *error);
//int execv(const char *program);//, char **args);

#endif /* PROCESS_SYSCALLS_H_ */
