/*
 * fileoperations.c
 *
 *  Created on: Feb 28, 2015
 *      Author: trinity
 */


#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include <kern/errno.h>
#include <kern/fcntl.h>

#include <fileoperations.h>
#include <kern/limits.h>
#include <synch.h>


int open(const char *filename, int flags)
{
	int err;

	// 1
	if(filename == NULL)
		err = EFAULT;

	// 2
	if(flags != O_RDONLY || flags != O_WRONLY || flags != O_RDWR)
		err = EINVAL;

	// 3
	struct fhandle *fh;
	int i;
	for(i=0; i<__OPEN_MAX; i++){
		if(curthread->t_fdtable[i] == NULL){
			curthread->t_fdtable[i] = create_fhandle(filename);
			fh = curthread->t_fdtable[i];
			break;
		}
	}
	if(i==__OPEN_MAX){
		err = EMFILE;
	}


	char* kfilename = kmalloc(sizeof(*filename));
	size_t *actual;

	err = copyinstr(filename, kfilename, sizeof(*filename),actual);
	if(sizeof(*filename) == *actual)
	{
		// checking whether its copied
	}

	lock_acquire(fh->mutex);

	err = vfs_open(filename, flags, O_CREAT, fh->vn);


	lock_release(fh->mutex);


	return 0;

}

struct fhandle* create_fhandle(const char* name)
{
	struct fhandle *fobj;

	fobj = kmalloc(sizeof(*fobj));
	if(fobj == NULL)
	{
		return NULL;
	}

	fobj->name = kmalloc(sizeof(256));
	if(fobj->name == NULL)
	{
		return NULL;
	}else
	{
		fobj->name = name;
	}

	fobj->flags = 0;
	fobj->offset = 0;
	fobj->ref_count = 0;

	fobj->mutex = lock_create(name);
	if(fobj->mutex == NULL)
	{
		return NULL;
	}

	return 0;

}

/*
int read(int filehandle, void *buf, size_t size);
int write(int filehandle, const void *buf, size_t size);
int close(int filehandle);
 */
