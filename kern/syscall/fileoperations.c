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

#include <lib.h>
#include <current.h>
#include <kern/limits.h>
#include <synch.h>
#include <kern/iovec.h>
#include <uio.h>
#include <vnode.h>
#include <vfs.h>
#include <fileoperations.h>
/*
 struct fhandle {
 char name[30];
 int flags;
 int offset;
 int ref_count;
 struct lock* mutex;
 struct vnode* vn;
 };
 */

struct fhandle* create_fhandle(const char* name) {
	struct fhandle *fobj;

	fobj = (struct fhandle*) kmalloc(sizeof(struct fhandle));
	if (fobj == NULL ) {
		return NULL ;
	}

	fobj->name = (const char*) kmalloc(sizeof(256));
	if (fobj->name == NULL ) {
		return NULL ;
	} else {
		fobj->name = name;
	}

	fobj->flags = 0;
	fobj->offset = 0;
	fobj->ref_count = 0;

	fobj->mutex = lock_create(name);
	if (fobj->mutex == NULL ) {
		return NULL ;
	}

	return 0;

}

void delete_fhandle(int fd) {
	struct fhandle *fh = curthread->t_fdtable[fd];
	//kfree(fh->name);
	lock_destroy(fh->mutex);
	kfree(fh->vn);
	kfree(fh);
	curthread->t_fdtable[fd] = NULL;
}

int open(const char *filename, int flags, int mode, int *error) {
	struct fhandle *fh;
	int fd = 0;

	// 1
	if (filename == NULL )
		*error = EFAULT;

	// 2
	if (flags != O_RDONLY || flags != O_WRONLY || flags != O_RDWR)
		*error = EINVAL;

	// 3

	int i;
	for (i = 0; i < __OPEN_MAX; i++) {
		if (curthread->t_fdtable[i] == NULL ) {
			curthread->t_fdtable[i] = create_fhandle(filename);
			fh = curthread->t_fdtable[i];
			fd = i;
			break;
		}
	}
	if (i == __OPEN_MAX) {
		*error = EMFILE;
	}

	char* kfilename = (char*) kmalloc(sizeof(*filename));
	size_t *actual;

	*error = copyinstr((userptr_t) filename, kfilename, sizeof(*filename),
			actual);
	if (sizeof(*filename) == *actual) {
		// checking whether its copied
	}

	lock_acquire(fh->mutex);
	*error = vfs_open((char*) filename, flags, mode, &fh->vn);
	lock_release(fh->mutex);

	return fd;
}

int close(int fd) {
	struct fhandle *fh;
	if (curthread->t_fdtable[fd] == NULL ) {
		return EBADF;
	} else {
		fh = curthread->t_fdtable[fd];
		lock_acquire(fh->mutex);
		fh->ref_count--;
		if (fh->ref_count == 0) {
			vfs_close(fh->vn);
			delete_fhandle(fd);
		}
		lock_release(fh->mutex);
		return 0;
	}
	return 0;
}

int read(int fd, void *buf, size_t size, int* error) {

	struct fhandle *fh;
	if (curthread->t_fdtable[fd] == NULL ) {
		*error = EBADF;
		return -1;
	} else {
		lock_acquire(fh->mutex);
		struct iovec *iovec_obj;
		struct uio *uio_obj;
		uio_init(iovec_obj, uio_obj, (void *) buf, size, fh->offset, UIO_READ);
		VOP_READ(fh->vn, uio_obj);

		int bytes_processed = size - uio_obj->uio_resid;
		fh->offset += bytes_processed;
		lock_release(fh->mutex);
		return bytes_processed;
	}
	return 0;
}

int write(int fd, const void *buf, size_t size, int* error) {

	struct fhandle *fh;
	if (curthread->t_fdtable[fd] == NULL ) {
		*error = EBADF;
		return -1;
	} else {
		lock_acquire(fh->mutex);
		struct iovec *iovec_obj;
		struct uio *uio_obj;
		uio_init(iovec_obj, uio_obj, (void *) buf, size, fh->offset, UIO_WRITE);
		VOP_WRITE(fh->vn, uio_obj);

		int bytes_processed = size - uio_obj->uio_resid;
		fh->offset += size - bytes_processed;
		lock_release(fh->mutex);
		return bytes_processed;
	}
	return 0;
}

