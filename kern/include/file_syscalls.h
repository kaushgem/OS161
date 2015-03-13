/*
 * file_syscalls.h
 *
 *  Created on: Feb 28, 2015
 *      Author: trinity
 */

#ifndef FILE_SYSCALLS_H_
#define FILE_SYSCALLS_H_



struct fhandle {
	char *name;
	int flags;
	off_t offset;
	int ref_count;
	struct lock* mutex;
	struct vnode* vn;
};

struct fhandle* create_fhandle(const char* name);
void delete_fhandle(int fd);

int open(const char *filename, int flags, int mode, int *error);
int read(int fd, void *buf, size_t size, int *error);
int write(int fd, const void *buf, size_t size, int *error);
int close(int fd);

int dup2(int oldfd, int newfd, int *error);
off_t lseek(int fd, off_t pos, int whence, int *error);

int chdir(const char *pathname);
int __getcwd(char *buf, size_t buflen, int *error);

#endif /* FILE_SYSCALLS_H_ */
