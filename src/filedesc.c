#include "quartz.h"
#include "filedesc.h"

quartz_Errno quartzFD_uFunc(quartz_Thread *Q, void *userdata, quartz_UserOp op) {
	quartz_fd *fd = (quartz_fd *)userdata;
	if(op == QUARTZ_USER_DESTROY) {
		return quartzFD_closefd(Q, fd);
	}
	return QUARTZ_OK;
}

static const char *quartzFD_typestr = "FILE*";

quartz_fd *quartzFD_tofd(quartz_Thread *Q, int x, quartz_Errno *err) {
	return (quartz_fd *)quartz_touserdata(Q, x, quartzFD_typestr, err);
}

quartz_Errno quartzFD_pushfd(quartz_Thread *Q, quartz_fd **outPtr) {
	return quartz_pushuserdata(Q, sizeof(quartz_fd), (void **)outPtr, quartzFD_typestr, quartzFD_uFunc);
}

quartz_Errno quartzFD_closefd(quartz_Thread *Q, quartz_fd *fd) {
	quartz_Errno err;
	if(fd->stdf != QUARTZ_STDFILE_COUNT) {
		// if 2 instances of the stdfile exist this can cause issues
		// however, the stdlib simply *does not allow this to ever happen*
		// we are closing a std file, this is bad
		quartz_File *f = quartz_fstdfile(Q, fd->stdf);
		if(f == NULL) return QUARTZ_OK; // already closed
		err = quartz_fclose(Q, f);
		if(err) return err;
		quartz_fsetstdfile(Q, fd->stdf, NULL);
		fd->stdf = QUARTZ_STDFILE_COUNT;
		fd->f = NULL;
		return QUARTZ_OK;
	}
	if(fd->f == NULL) return QUARTZ_OK;
	err = quartz_fclose(Q, fd->f);
	if(err) return err;
	fd->f = NULL;
	return QUARTZ_OK;
}

quartz_File *quartzFD_unwrapfd(quartz_Thread *Q, quartz_fd *fd) {
	if(fd->stdf == QUARTZ_STDFILE_COUNT) {
		return fd->f;
	}
	return quartz_fstdfile(Q, fd->stdf);
}
