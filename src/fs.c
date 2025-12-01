#include "quartz.h"
#include "value.h"
#include "context.h"
#include "utils.h"

typedef struct quartz_File {
	void *fData;
	char *buf;
	size_t bufcursor;
	size_t buflen;
	size_t bufsize;
	quartz_FsBufMode bufMode;
	quartz_FileMode fMode;
} quartz_File;

static quartz_File *quartz_fwrap(quartz_Thread *Q, void *fData, quartz_FileMode mode, quartz_Errno *err) {
	size_t defaultBufSize = Q->gState->context.fsDefaultBufSize;
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
	f->bufcursor = 0;
	f->bufsize = defaultBufSize;
	f->fData = fData;
	f->fMode = mode;
	f->bufMode = QUARTZ_FS_LINEBUF;
	return f;
}

quartz_File *quartz_fopen(quartz_Thread *Q, const char *path, quartz_FileMode mode, quartz_Errno *err) {
	quartz_Context ctx = Q->gState->context;
	void *fData;

	*err = ctx.fs(Q, ctx.userdata, &fData, QUARTZ_FS_OPEN, (void *)path, (void *)&mode);
	if(*err) return NULL;

	quartz_File *f = quartz_fwrap(Q, fData, mode, err);
	if(f == NULL) {
		// welp, too bad
		ctx.fs(Q, ctx.userdata, &fData, QUARTZ_FS_CLOSE, NULL, NULL);
		return NULL;
	}
	return f;
}

void quartz_ffree(quartz_Thread *Q, quartz_File *f) {
	quartz_free(Q, f->buf, f->bufsize);
	quartz_free(Q, f, sizeof(quartz_File));
}

quartz_Errno quartz_fclose(quartz_Thread *Q, quartz_File *f) {
	quartz_Context ctx = Q->gState->context;
	quartz_Errno err = ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_CLOSE, NULL, NULL);
	if(err) return err;
	quartz_ffree(Q, f);
	return QUARTZ_OK;
}

quartz_Errno quartz_fwrite(quartz_Thread *Q, quartz_File *f, const void *buf, size_t *buflen) {
	if(f->fMode & QUARTZ_FILE_READABLE) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "bad file mode");
	}
	quartz_Context ctx = Q->gState->context;
	if(f->bufMode == QUARTZ_FS_NOBUF) {
		return ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_WRITE, (void *)buf, buflen);
	}
	size_t remaining = f->bufsize - f->buflen;
	bool needsFlushing = (*buflen) > remaining;
	if(f->bufMode == QUARTZ_FS_LINEBUF) {
		size_t len = *buflen;
		const char *chars = buf;
		for(size_t i = 0; i < len; i++) {
			if(chars[i] == '\n') {
				needsFlushing = true;
				break;
			}
		}
	}
	if(needsFlushing) {
		size_t written = f->buflen + *buflen;
		// too much at once, just flush all
		// TODO: the common case is the buffer barely fills, in which case it doesn't matter much
		quartz_Errno err = quartz_fflush(Q, f);
		if(err) return err;
		err = ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_WRITE, (void *)buf, buflen);
		if(err) return err;
		*buflen = written;
		return QUARTZ_OK;
	}
	// should be safe by now
	quartzI_memcpy(f->buf + f->buflen, buf, *buflen);
	f->buflen += *buflen;
	// all data written
	return QUARTZ_OK;
}

// returns if EoF
static bool quartzI_fgetc(quartz_Thread *Q, quartz_File *f, char *buf, quartz_Errno *err) {
	quartz_Context ctx = Q->gState->context;
	if(f->bufcursor == f->buflen) {
		// buffer exhausted
		f->bufcursor = 0;
		f->buflen = f->bufsize;
		*err = ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_READ, f->buf, &f->buflen);
		if(*err) {
			// erased (TODO: evaluate if this is good)
			f->buflen = 0;
			return true;
		}
		// EoF
		if(f->buflen == 0) {
			// gone
			return true;
		}
	}
	*buf = f->buf[f->bufcursor++];
	*err = QUARTZ_OK;
	return false;
}

quartz_Errno quartz_fread(quartz_Thread *Q, quartz_File *f, void *buf, size_t *buflen) {
	if((f->fMode & QUARTZ_FILE_READABLE) != QUARTZ_FILE_READABLE) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "bad file mode");
	}
	if(f->bufMode == QUARTZ_FS_NOBUF) {
		// this is ultra free
		quartz_Context ctx = Q->gState->context;
		return ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_READ, buf, buflen);
	}
	size_t requested = *buflen;
	size_t written = requested;
	for(size_t i = 0; i < requested; i++) {
		quartz_Errno err;
		if(quartzI_fgetc(Q, f, buf + i, &err)) {
			written = i;
			break;
		}
		if(err) return err;
	}
	*buflen = written;
	return QUARTZ_OK;
}

quartz_Errno quartz_fseek(quartz_Thread *Q, quartz_File *f, quartz_FsSeekWhence whence, quartz_Int off, size_t *cursor) {
	quartz_Context ctx = Q->gState->context;
	quartz_Errno err = quartz_fflush(Q, f);
	if(err) return err;
	quartz_FsSeekArg arg = {
		.whence = whence,
		.off = off,
	};
	err = ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_SEEK, &arg, cursor);
	return QUARTZ_OK;
}

quartz_Errno quartz_fflush(quartz_Thread *Q, quartz_File *f) {
	if(f->fMode & QUARTZ_FILE_READABLE) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "bad file mode");
	}
	quartz_Context ctx = Q->gState->context;
	quartz_Errno err = ctx.fs(Q, ctx.userdata, &f->fData, QUARTZ_FS_WRITE, (void *)f->buf, &f->buflen);
	if(err) return err;
	f->buflen = 0;
	f->bufcursor = 0;
	return QUARTZ_OK;
}

quartz_Errno quartz_fsetvbuf(quartz_Thread *Q, quartz_File *f, quartz_FsBufMode bufMode, size_t size) {
	if((f->fMode & QUARTZ_FILE_READABLE) == 0) {
		quartz_Errno err = quartz_fflush(Q, f);
		if(err) return err;
	}
	f->bufMode = bufMode;
	if(size == 0) size = f->bufsize;
	if(size != f->bufsize) {
		char *newBuf = quartz_realloc(Q, f->buf, f->bufsize, size);
		if(newBuf == NULL) return quartz_oom(Q);
		f->buf = newBuf;
		f->bufsize = size;
	}
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

	quartz_File *_stdin = quartz_fstdfile(Q, QUARTZ_STDIN);
	err = quartz_fsetvbuf(Q, _stdin, QUARTZ_FS_NOBUF, 0);
	if(err) return err;

	return QUARTZ_OK;
}

quartz_Errno quartz_freadline(quartz_Thread *Q, quartz_File *f, quartz_Buffer *buf) {
	quartz_Errno err = QUARTZ_OK;

	while(true) {
		char b;
		size_t len = 1;
		err = quartz_fread(Q, f, &b, &len);
		if(err) return err;
		if(len == 0) break;
		err = quartz_bufputc(buf, b);
		if(err) return err;
		if(b == '\n') break;
	}

	return QUARTZ_OK;
}

quartz_Errno quartz_freadall(quartz_Thread *Q, quartz_File *f, quartz_Buffer *buf) {
	quartz_Errno err = QUARTZ_OK;
	char cbuf[1024];
	size_t bufsize = sizeof(cbuf);

	while(true) {
		size_t len = bufsize;
		err = quartz_fread(Q, f, cbuf, &len);
		if(err) return err;
		if(len == 0) break;
		err = quartz_bufputls(buf, cbuf, len);
		if(err) return err;
	}

	return QUARTZ_OK;
}
