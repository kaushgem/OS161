/*
 * fileoperations.h
 *
 *  Created on: Feb 28, 2015
 *      Author: trinity
 */

#ifndef FILEOPERATIONS_H_
#define FILEOPERATIONS_H_



struct fhandle {
	char name[30];
	int flags;
	int offset;
	int ref_count;
	struct lock* mutex;
	struct vnode* vn;
};

int open(const char *filename, int flags, ...);
int read(int filehandle, void *buf, size_t size);
int write(int filehandle, const void *buf, size_t size);
int close(int filehandle);


#endif /* FILEOPERATIONS_H_ */
