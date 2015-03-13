/*
 * file_syscalls.c
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
#include <kern/seek.h>
#include <synch.h>
#include <kern/iovec.h>
#include <uio.h>
#include <vnode.h>
#include <vfs.h>
#include <file_syscalls.h>
#include <kern/stat.h>


struct fhandle* create_fhandle(const char* name) {

	struct fhandle *fobj;

	fobj = kmalloc(sizeof(struct fhandle));
	if (fobj == NULL ) {
		return NULL ;
	}

	fobj->name = kstrdup(name);
	if (fobj->name == NULL) {
		kfree(fobj);
		return NULL;
	}

	fobj->flags = 0;
	fobj->offset = 0;
	fobj->ref_count = 1;

	fobj->mutex = lock_create(name);
	if (fobj->mutex == NULL ) {
		kfree(fobj->name);
		kfree(fobj);
		return NULL ;
	}
	return fobj;
}

void delete_fhandle(int fd) {
	struct fhandle *fh = curthread->t_fdtable[fd];
	lock_destroy(fh->mutex);
	kfree(fh->name);
	//kfree(fh->vn);
	kfree(fh);
	curthread->t_fdtable[fd] = NULL;
}

int open(const char *filename, int flags, int mode, int *error) {
	struct fhandle *fh;
	int fd = 0;

	if (filename == NULL )
	{
		*error = EFAULT;
		return -1;
	}

	int rwmask = flags&O_ACCMODE;

	if (rwmask != O_RDWR &&  rwmask != O_WRONLY && rwmask != O_RDONLY )
	{
		*error = EINVAL;
		return -1;
	}

	/*if(filename == (char *)0x40000000 || filename == (char *)0x80000000){
	 *error = EFAULT;
		return -1;
	}*/
	char kfilename[__NAME_MAX];
	size_t actual;

	*error = copyinstr((const_userptr_t) filename, kfilename, __NAME_MAX, &actual);
	if(*error != 0){
		return -1;
	}

	int i;
	for (i = 3; i < __OPEN_MAX; i++) {
		if (curthread->t_fdtable[i] == NULL ) {
			curthread->t_fdtable[i] = create_fhandle(filename);
			fh = curthread->t_fdtable[i];
			fd = i;
			break;
		}
	}
	if (i == __OPEN_MAX) {
		*error = EMFILE;
		return -1;
	}




	if(strlen(filename)==0)
	{
		*error = EINVAL;
		return -1;
	}

	lock_acquire(fh->mutex);
	*error = vfs_open((char*) kfilename, flags, mode, &fh->vn);
	lock_release(fh->mutex);
	if(*error != 0){
		return -1;
	}

	return fd;
}

int close(int fd) {

	if (fd < 0 || fd > __OPEN_MAX) {
		return EBADF;
	} else {
		struct fhandle *fh = curthread->t_fdtable[fd];
		if(fh==NULL)
			return EBADF;
		lock_acquire(fh->mutex);
		fh->ref_count--;

		if (fh->ref_count == 0) {
			vfs_close(fh->vn);
			lock_release(fh->mutex);
			delete_fhandle(fd);
			return 0;
		}
		lock_release(fh->mutex);
		return 0;
	}
	return 0;
}

int read(int fd, void *buf, size_t size, int* error) {

	if ( fd < 0 || fd > __OPEN_MAX) {
		*error = EBADF;
		return -1;
	} else if (buf == NULL) {
		*error = EFAULT;
		return -1;
	} else {

		struct fhandle *fh = curthread->t_fdtable[fd];
		if(fh==NULL)
		{
			*error = EBADF;
			return -1;
		}



		char kfilename[__NAME_MAX];
		size_t actual;
		*error = copyinstr((const_userptr_t) buf, kfilename, __NAME_MAX, &actual);
		if(*error != 0){
			return -1;
		}




		lock_acquire(fh->mutex);
		struct iovec iovec_obj;
		struct uio uio_obj;
		uio_init(&iovec_obj, &uio_obj, (void *) buf, size, fh->offset, UIO_READ);
		*error = VOP_READ(fh->vn, &uio_obj);
		if(*error != 0){
			lock_release(fh->mutex);
			kfree((void*)buf);
			return -1;
		}
		int bytes_processed = uio_obj.uio_offset -fh->offset;
		fh->offset = uio_obj.uio_offset;
		lock_release(fh->mutex);
		return bytes_processed;
	}
	return 0;
}

int write(int fd, const void *buf, size_t size, int* error) {

	if ( fd < 0 || fd > __OPEN_MAX) {
		*error = EBADF;
		return -1;
	} else if (buf == NULL) {
		*error = EFAULT;
		return -1;
	} else {


		struct fhandle *fh = curthread->t_fdtable[fd];
		if (fh == NULL )
		{
			*error = EBADF;
			return -1;
		}

		char kfilename[__NAME_MAX];
		size_t actual;
		*error = copyinstr((const_userptr_t) buf, kfilename, __NAME_MAX, &actual);
		if(*error != 0){
			return -1;
		}



		lock_acquire(fh->mutex);
		struct iovec iovec_obj;
		struct uio uio_obj;
		uio_init(&iovec_obj, &uio_obj, (void *) buf, size, fh->offset, UIO_WRITE);
		*error = VOP_WRITE(fh->vn, &uio_obj);
		if(*error != 0){
			lock_release(fh->mutex);
			kfree((void*)buf);
			return -1;
		}
		int bytes_processed = size - uio_obj.uio_resid;
		fh->offset = uio_obj.uio_offset;
		//kprintf("\n Byte processed %d     offset: %d", bytes_processed, (int)fh->offset);
		lock_release(fh->mutex);
		return bytes_processed;
	}
	return 0;
}


int dup2(int oldfd, int newfd , int *error){

	if(	oldfd <0 ||
			oldfd>__OPEN_MAX ||
			newfd<0 ||
			curthread->t_fdtable[oldfd] == NULL ||
			newfd >=__OPEN_MAX 	)
	{
		*error = EBADF;
		return -1;
	}
	else
	{
		if(curthread->t_fdtable[newfd] != NULL)
		{
			*error = close(newfd);
			if(*error >0)
				return -1;
		}

		curthread->t_fdtable[newfd] = curthread->t_fdtable[oldfd];
		lock_acquire(curthread->t_fdtable[newfd]->mutex);
		curthread->t_fdtable[newfd]->ref_count++;
		lock_release(curthread->t_fdtable[newfd]->mutex);
	}
	return oldfd;
}


off_t lseek(int fd, off_t pos, int whence , int *error)
{
	struct fhandle *fh ;
	if ( fd < 0 || fd >= 128) {
		*error = EBADF;
		return -1;
	}
	else if(whence != SEEK_SET
			&& whence !=SEEK_CUR
			&& whence!=SEEK_END)
	{
		*error = EINVAL;
		return -1;
	}
	else
	{
		// valid lseek
		fh = curthread->t_fdtable[fd];
		if(fh==NULL)
		{
			*error = EBADF;
			return -1;

		}
		struct stat st;
		int position_new = fh->offset;

		switch(whence){
		case SEEK_SET:
			position_new = pos;
			break;
		case SEEK_CUR:
			position_new = fh->offset + pos;
			break;
		case SEEK_END:
			VOP_STAT(fh->vn,&st);
			position_new = st.st_size + pos;
			break;
		}

		if(position_new < 0){
			*error = EINVAL;
			return -1;
		}

		lock_acquire(fh->mutex);
		*error = VOP_TRYSEEK(fh->vn, position_new);
		lock_release(fh->mutex);
		if(*error != 0){
			return -1;
		}

		fh->offset = position_new;
		//kprintf("\nOffset: %ld  position: %ld",(long)fh->offset,(long)pos);
		return fh->offset;
	}
	return fh->offset;
}

int chdir(const char *pathname)
{
	if(pathname == NULL)
	{
		return EFAULT;
	}

	char k_pathname[__PATH_MAX];
	size_t actual;
	int err = copyinstr((const_userptr_t) pathname, k_pathname, __PATH_MAX, &actual);
	if(err != 0){
		return err;
	}
	return vfs_chdir((char*)pathname);
}

int __getcwd(char *buf, size_t buflen, int *error)
{
	if(buf==NULL){
		*error = EFAULT ;
		return -1;
	}

	char k_pathname[__PATH_MAX];
	size_t actual;
	*error = copyinstr((const_userptr_t) buf, k_pathname, __PATH_MAX, &actual);
	if(*error != 0){
		return -1;
	}


	struct iovec iovec_obj;
	struct uio uio_obj;
	uio_init(&iovec_obj, &uio_obj, (void *) buf, buflen, 0, UIO_READ);
	*error = vfs_getcwd(&uio_obj);
	return buflen - uio_obj.uio_resid;


}

