#include "context.h"

#ifndef QUARTZ_NO_LIBC
#include <stdlib.h>
#include <stdio.h>
#endif

size_t quartz_sizeOfContext() {
	return sizeof(quartz_Context);
}

static void *quartzI_defaultAlloc(void *userdata, void *memory, size_t oldSize, size_t newSize) {
#ifdef QUARTZ_NO_LIBC
	return NULL; // OOM
#else
	if(newSize == 0) {
		free(memory);
		return NULL;
	}
	return realloc(memory, newSize);
#endif
}

static quartz_Errno quartzI_defaultFile(quartz_Thread *Q, void *userdata, void **fileData, quartz_FsAction action, void *buf, size_t *buflen) {
#ifdef QUARTZ_NO_LIBC
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "filesystem is not defined, I/O is limited");
#else
	if(action == QUARTZ_FS_STDFILE) {
		quartz_StdFile s = *(quartz_StdFile *)buf;
		FILE *files[QUARTZ_STDFILE_COUNT] = {
			[QUARTZ_STDIN] = stdin,
			[QUARTZ_STDOUT] = stdout,
			[QUARTZ_STDERR] = stderr,
		};
		*fileData = files[s];
		return QUARTZ_OK;
	}
	if(action == QUARTZ_FS_WRITE) {
		FILE *f = *(FILE **)fileData;
		*buflen = fwrite(buf, 1, *buflen, f);
		// we could check by length but doing so is weird
		return QUARTZ_OK;
	}
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "filesystem is action is unsupported, something is horribly wrong");
#endif
}

void quartz_initContext(quartz_Context *ctx, void *userdata) {
	ctx->userdata = userdata;
	ctx->alloc = quartzI_defaultAlloc;
	ctx->fs = quartzI_defaultFile;
#ifdef QUARTZ_NO_LIBC
	ctx->fsDefaultBufSize = 1024;
#else
	ctx->fsDefaultBufSize = BUFSIZ;
#endif
}

void *quartz_rawAlloc(quartz_Context *ctx, size_t size) {
	return ctx->alloc(ctx->userdata, NULL, 0, size);
}

void *quartz_rawRealloc(quartz_Context *ctx, void *memory, size_t oldSize, size_t newSize) {
	return ctx->alloc(ctx->userdata, memory, oldSize, newSize);
}

void quartz_rawFree(quartz_Context *ctx, void *memory, size_t size) {
	ctx->alloc(ctx->userdata, memory, size, 0);
}

double quartz_rawClock(quartz_Context *ctx) {
	return ctx->clock(ctx->userdata);
}

size_t quartz_rawTime(quartz_Context *ctx) {
	return ctx->time(ctx->userdata);
}

void quartz_setAllocator(quartz_Context *ctx, quartz_Allocf alloc) {
	ctx->alloc = alloc;
}

void quartz_setClock(quartz_Context *ctx, quartz_Clockf clock) {
	ctx->clock = clock;
}

void quartz_setTime(quartz_Context *ctx, quartz_Timef time) {
	ctx->time = time;
}

void quartz_setFileSystem(quartz_Context *ctx, quartz_Filef file, size_t defaultFileBufSize) {
	ctx->fs = file;
	ctx->fsDefaultBufSize = defaultFileBufSize;
}
