/*
 * fileoperations.c
 *
 *  Created on: Feb 28, 2015
 *      Author: trinity
 */


#include <types.h>
#include <copyinout.h>
#include <syscall.h>


int open(const char *filename, int flags)
{
	int result;

	if(filename == NULL)
		result = EFAULT;

	if(flags != O_RDONLY || flags != O_WRONLY || flags != O_RDWR)
		result = EINVAL;




	result = copyout(&seconds, user_seconds_ptr, sizeof(time_t));
	if (result) {
		return result;
	}

	result = copyout(&nanoseconds, user_nanoseconds_ptr, sizeof(uint32_t));
	if (result) {
		return result;
	}

	return 0;
}

/*
int read(int filehandle, void *buf, size_t size);
int write(int filehandle, const void *buf, size_t size);
int close(int filehandle);
 */
