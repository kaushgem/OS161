/*
 * fileoperations.h
 *
 *  Created on: Feb 28, 2015
 *      Author: trinity
 */

#ifndef FILEOPERATIONS_H_
#define FILEOPERATIONS_H_



struct fhandle {
	const char *name;
	int flags;
	int offset;
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



#endif /* FILEOPERATIONS_H_ */
