#include "quartz.h"
#include "value.h"
#include "context.h"

typedef struct quartz_File {
	void *fData;
	char *buf;
	size_t buflen;
	size_t bufsize;
	quartz_FsBufMode bufMode;
	quartz_FileMode fMode;
} quartz_File;

static quartz_File *quartz_fwrap(quartz_Thread *Q, void *fData, quartz_FileMode mode, quartz_Errno *err) {
	size_t defaultBufSize = 8192;
	char *buf = quartz_alloc(Q, defaultBufSize);
	if(buf == NULL) {
		*err = quartz_oom(Q);
		return NULL;
	}
	quartz_File *f = quartz_alloc(Q, sizeof(quartz_File));
	if(f == NULL) {
		*err = quartz_oom(Q);
		quartz_free(Q, buf, defaultBufSize);
		return NULL;
	}
	f->buf = buf;
	f->buflen = 0;
	f->bufsize = defaultBufSize;
	f->fData = fData;
	f->fMode = mode;
	f->bufMode = QUARTZ_FS_LINEBUF;
	return f;
}

quartz_File *quartz_fopen(quartz_Thread *Q, const char *path, size_t pathlen, quartz_FileMode mode, quartz_Errno *err) {
	quartz_Context ctx = Q->gState->context;
	void *fData;

	*err = ctx.fs(Q, ctx.userdata, &fData, QUARTZ_FS_OPEN, (void *)path, &pathlen);
	if(*err) return NULL;

	quartz_File *f = quartz_fwrap(Q, fData, mode, err);
	if(f == NULL) {
		// welp, too bad
		ctx.fs(Q, ctx.userdata, &fData, QUARTZ_FS_CLOSE, NULL, NULL);
		return NULL;
	}
	return f;
}

quartz_Errno quartz_fclose(quartz_Thread *Q, quartz_File *f) {
	quartz_Context ctx = Q->gState->context;
	quartz_Errno err = ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_CLOSE, NULL, NULL);
	if(err) return err;
	quartz_free(Q, f->buf, f->bufsize);
	quartz_free(Q, f, sizeof(quartz_File));
	return QUARTZ_OK;
}

quartz_Errno quartz_fwrite(quartz_Thread *Q, quartz_File *f, const void *buf, size_t *buflen);
quartz_Errno quartz_fread(quartz_Thread *Q, quartz_File *f, void *buf, size_t *buflen);
quartz_Errno quartz_fseek(quartz_Thread *Q, quartz_File *f, quartz_FsSeekWhence whence, quartz_Int off);

quartz_Errno quartz_fflush(quartz_Thread *Q, quartz_File *f) {
	quartz_Context ctx = Q->gState->context;
	quartz_Errno err = ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_WRITE, (void *)f->buf, &f->buflen);
	if(err) return err;
	f->buflen = 0;
	return QUARTZ_OK;
}

quartz_Errno quartz_fsetvbuf(quartz_Thread *Q, quartz_File *f, quartz_FsBufMode bufMode, size_t size) {
	quartz_Errno err = quartz_fflush(Q, f);
	if(err) return err;
	return QUARTZ_OK;
}

quartz_File *quartz_fstdfile(quartz_Thread *Q, quartz_StdFile stdFile) {
	return Q->gState->stdfiles[stdFile];
}

void quartz_fsetstdfile(quartz_Thread *Q, quartz_StdFile stdFile, quartz_File *f) {
	Q->gState->stdfiles[stdFile] = f;
}

static quartz_Errno quartz_fopenstdfile(quartz_Thread *Q, quartz_FileMode fmode, quartz_StdFile stdf) {
	quartz_Context ctx = Q->gState->context;
	void *fData;
	quartz_Errno err = ctx.fs(Q, ctx.userdata, &fData, QUARTZ_FS_STDFILE, &stdf, NULL);
	if(err) return err;
	quartz_File *f = quartz_fwrap(Q, fData, fmode, &err);
	if(f == NULL) return err;
	quartz_fsetstdfile(Q, stdf, f);
	return QUARTZ_OK;
}

quartz_Errno quartz_fopenstdio(quartz_Thread *Q) {
	quartz_Errno err;
	err = quartz_fopenstdfile(Q, QUARTZ_FILE_READABLE, QUARTZ_STDIN);
	if(err) return err;
	err = quartz_fopenstdfile(Q, QUARTZ_FILE_WRITABLE, QUARTZ_STDOUT);
	if(err) return err;
	err = quartz_fopenstdfile(Q, QUARTZ_FILE_WRITABLE, QUARTZ_STDERR);
	if(err) return err;
	return QUARTZ_OK;
}
