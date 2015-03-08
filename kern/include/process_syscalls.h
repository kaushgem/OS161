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

struct process_block
{
	pid_t parent_pid;
	struct cv *process_cv;
	struct lock *cv_lock;
	int exitcode;
	struct thread* t;
	struct fhandle* fdtable[__OPEN_MAX];
};

pid_t getpid_(void);

#endif /* PROCESS_SYSCALLS_H_ */
