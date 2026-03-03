#include <stddef.h>
#include <stdint.h>
#include "quartz.h"
#include "common.h"

#ifndef QUARTZ_NOLIBC
#include <stdlib.h>
#include <stdio.h>
#endif

size_t qrtz_strlen(const char *s) {
	size_t len = 0;
	while(s[len]) len++;
	return len;
}

char *qrtz_strdup(qrtz_Context *ctx, const char *s) {
	size_t len = qrtz_strlen(s);
	char *n = qrtz_alloc(ctx, len + 1);
	if(n == NULL) return NULL;
	qrtz_memcpy(n, s, len);
	n[len] = '\0';
	return n;
}

void qrtz_strfree(qrtz_Context *ctx, char *s) {
	size_t len = qrtz_strlen(s);
	qrtz_free(ctx, s, len + 1);
}

void qrtz_memcpy(void *dest, const void *src, size_t len) {
	char *destBytes = (char *)dest;
	const char *srcBytes = (const char *)src;

	for(size_t i = 0; i < len; i++) destBytes[i] = srcBytes[i];
}

size_t qrtz_sizeofContext() {
	return sizeof(qrtz_Context);
}

static void *qrtz_defaultAlloc(void *_, void *mem, size_t oldSize, size_t newSize) {
	(void)_;
	(void)oldSize;
#ifdef QUARTZ_NOLIBC
	(void)mem;
	(void)newSize;
#else
	if(newSize == 0) {
		free(mem);
		return NULL;
	}

	return realloc(mem, newSize);
#endif
}

void qrtz_initContext(qrtz_Context *ctx) {
	ctx->data = NULL;
	ctx->alloc = qrtz_defaultAlloc;
}

bool qrtz_sizeOverflows(size_t a, size_t b) {
	if(b == 0) return false;
	return a > (SIZE_MAX/b);
}

void *qrtz_alloc(qrtz_Context *ctx, size_t len) {
	if(len == 0) return ctx->alloc;
	return ctx->alloc(ctx->data, NULL, 0, len);
}

void *qrtz_allocArray(qrtz_Context *ctx, size_t len, size_t count) {
	if(qrtz_sizeOverflows(len, count)) return NULL;
	return qrtz_alloc(ctx, len * count);
}

void qrtz_free(qrtz_Context *ctx, void *memory, size_t len) {
	if(memory == ctx->alloc) return;
	if(memory == NULL) return;
	(void)ctx->alloc(ctx->data, memory, len, 0);
}

void qrtz_freeArray(qrtz_Context *ctx, void *memory, size_t len, size_t count) {
	return qrtz_free(ctx, memory, len * count);
}

void *qrtz_realloc(qrtz_Context *ctx, void *memory, size_t len, size_t oldCount, size_t newCount) {
	if(qrtz_sizeOverflows(len, newCount)) return NULL;
	size_t oldSize = len * oldCount;
	size_t newSize = len * newCount;
	if(memory == ctx->alloc) return qrtz_alloc(ctx, newSize);
	return ctx->alloc(ctx->data, memory, oldSize, newSize);
}

void qrtz_initBuf(qrtz_Buffer *buf, qrtz_Context *ctx) {
	buf->ctx = ctx;
	buf->buf = NULL;
	buf->len = 0;
	buf->cap = 0;
}

void qrtz_freeBuf(qrtz_Buffer *buf) {
	qrtz_freeArray(buf->ctx, buf->buf, sizeof(char), buf->cap);
}

qrtz_Exit qrtz_initBufCap(qrtz_Buffer *buf, qrtz_Context *ctx, size_t cap) {
	char *mem = qrtz_allocArray(ctx, sizeof(char), cap);
	if(mem == NULL) return QRTZ_ENOMEM;
	buf->ctx = ctx;
	buf->buf = mem;
	buf->len = 0;
	buf->cap = cap;
	return QRTZ_OK;
}

char *qrtz_reserveBuf(qrtz_Buffer *buf, size_t amount) {
	if(buf->ctx == NULL) {
		// special case
		if(buf->len + amount > buf->cap) return NULL;
		char *mem = buf->buf + buf->len;
		buf->len += amount;
		return mem;
	}
	size_t needed = buf->len + amount;
	size_t capNeeded = buf->cap;
	while(capNeeded < needed) capNeeded *= 2;
	if(capNeeded > buf->cap) {
		char *newMem = qrtz_realloc(buf->ctx, buf->buf, sizeof(char), buf->cap, capNeeded);
		if(newMem == NULL) return NULL;
		buf->buf = newMem;
		buf->cap = capNeeded;
	}

	char *mem = buf->buf + buf->len;
	buf->len += amount;
	return mem;
}

qrtz_Exit qrtz_putc(qrtz_Buffer *buf, char c) {
	char *p = qrtz_reserveBuf(buf, 1);
	if(p == NULL) return QRTZ_ENOMEM;
	*p = c;
	return QRTZ_OK;
}

qrtz_Exit qrtz_putcn(qrtz_Buffer *buf, char c, size_t rep) {
	char *p = qrtz_reserveBuf(buf, rep);
	if(p == NULL) return QRTZ_ENOMEM;
	for(size_t i = 0; i < rep; i++) p[i] = c;
	return QRTZ_OK;
}

qrtz_Exit qrtz_puts(qrtz_Buffer *buf, const char *s) {

}

qrtz_Exit qrtz_putls(qrtz_Buffer *buf, const char *s, size_t len);
qrtz_Exit qrtz_putf(qrtz_Buffer *buf, const char *fmt, ...);
qrtz_Exit qrtz_vputf(qrtz_Buffer *buf, const char *fmt, va_list args);
