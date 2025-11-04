#include "context.h"

#define QUARTZ_USE_LIBC

#ifdef QUARTZ_USE_LIBC
#include <stdlib.h>
#include <stdio.h>
#endif

size_t quartz_sizeOfContext() {
	return sizeof(quartz_Context);
}

static void *quartzI_defaultAlloc(void *userdata, void *memory, size_t oldSize, size_t newSize) {
#ifdef QUARTZ_USE_LIBC
	if(newSize == 0) {
		free(memory);
		return NULL;
	}
	return realloc(memory, newSize);
#else
	return NULL; // OOM
#endif
}

void quartz_initContext(quartz_Context *ctx, void *userdata) {
	ctx->userdata = userdata;
	ctx->alloc = quartzI_defaultAlloc;
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
