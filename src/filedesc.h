#ifndef QUARTZ_LIB_FILEDESC_H
#define QUARTZ_LIB_FILEDESC_H

#include "quartz.h"

typedef struct quartz_fd {
	// NULL if closed
	quartz_File *f;
	// if QUARTZ_STDFILE_COUNT, use f as the file desc
	// if not, f should be NULL and stdf should be, well, the stdf
	quartz_StdFile stdf;
} quartz_fd;

quartz_Errno quartzFD_uFunc(quartz_Thread *Q, void *userdata, quartz_UserOp op);
quartz_fd *quartzFD_tofd(quartz_Thread *Q, int x, quartz_Errno *err);
quartz_Errno quartzFD_pushfd(quartz_Thread *Q, quartz_fd **outPtr);
quartz_Errno quartzFD_closefd(quartz_Thread *Q, quartz_fd *fd);
// if NULL, the file is closed
quartz_File *quartzFD_unwrapfd(quartz_Thread *Q, quartz_fd *fd);

#endif
