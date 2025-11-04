#ifndef QUARTZ_H
#define QUARTZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef intptr_t quartz_Int;
typedef double quartz_Real;

typedef struct quartz_Complex {
	// float so it fits
	float real;
	float imaginary;
} quartz_Complex;

typedef enum quartz_Type {
	QUARTZ_TNULL = 0,
	QUARTZ_TBOOL,
	QUARTZ_TINT,
	QUARTZ_TREAL,
	QUARTZ_TCOMPLEX,
	QUARTZ_TSTR,
	QUARTZ_TLIST,
	QUARTZ_TTUPLE,
	QUARTZ_TSET,
	QUARTZ_TMAP,
	QUARTZ_TSTRUCT,
	QUARTZ_TFUNCTION,
	QUARTZ_TPOINTER,
	QUARTZ_TTHREAD,
	QUARTZ_TUSERDATA,
	
	QUARTZ_TCOUNT,
} quartz_Type;

extern const char *quartz_typenames[QUARTZ_TCOUNT];

typedef struct quartz_Thread quartz_Thread;

typedef enum quartz_Errno {
	QUARTZ_OK = 0,
	QUARTZ_YIELD = 1,
	// out of memory error. Error handler is not called for these.
	QUARTZ_ENOMEM,
	// I/O error.
	QUARTZ_EIO,
	// Syntax error
	QUARTZ_ESYNTAX,
	// Runtime error
	QUARTZ_ERUNTIME,
} quartz_Errno;

typedef void *(*quartz_Allocf)(void *userdata, void *memory, size_t oldSize, size_t newSize);
// returns some amount of elapsed time in seconds
typedef double (*quartz_Clockf)(void *userdata);
// returns the current UNIX timestamp
typedef size_t (*quartz_Timef)(void *userdata);

typedef struct quartz_Context quartz_Context;

size_t quartz_sizeOfContext();
void quartz_initContext(quartz_Context *ctx, void *userdata);
void quartz_setAllocator(quartz_Context *ctx, quartz_Allocf *alloc);
void quartz_setClock(quartz_Context *ctx, quartz_Clockf *clock);
void quartz_setTime(quartz_Context *ctx, quartz_Timef *time);

typedef quartz_Errno (*quartz_CFunction)(quartz_Thread *Q, size_t argc);
typedef quartz_Errno (*quartz_KFunction)(quartz_Thread *Q, quartz_Errno state, void *context);

typedef enum quartz_UserOp {
	// An attempt to read a field. Arg 1 stores the field name as a string. Should return the value, or an error.
	QUARTZ_USER_GETFIELD,
	// An attempt to set a field. Arg 1 stores the field name as a string, and arg 2 stores the value.
	QUARTZ_USER_SETFIELD,
	// The userdata has been reclaimed by the garbage collector and thus the resources should be released. Associated values remain usable.
	// The GC does *not* support reviving currently, DO NOT ASSIGN IT TO A NEW VALUE.
	QUARTZ_USER_DESTROY,
} quartz_UserOp;

typedef quartz_Errno (*quartz_UFunction)(quartz_Thread *Q, void *userdata, quartz_UserOp op);

// the stack size is in values
quartz_Thread *quartz_newThread(quartz_Context *ctx);
void quartz_destroyThread(quartz_Thread *Q);

// The current managed memory usage
size_t quartz_gcCount(quartz_Thread *Q);

// The memory at which GC will trigger
size_t quartz_gcTarget(quartz_Thread *Q);

// peak memory usage
size_t quartz_gcPeak(quartz_Thread *Q);

// set it to 0 to leave it unchanged
double quartz_gcRatio(quartz_Thread *Q, double ratio);

void quartz_gc(quartz_Thread *Q);

// These functions do use the context, but they may trigger GC to best use memory.
// Prefer using them in low-memory environments
void *quartz_alloc(quartz_Thread *Q, size_t size);
void *quartz_realloc(quartz_Thread *Q, void *memory, size_t oldSize, size_t newSize);
void quartz_free(quartz_Thread *Q, void *memory, size_t size);

double quartz_clock(quartz_Thread *Q);
size_t quartz_time(quartz_Thread *Q);

#define QUARTZ_MIN_BASE 2
#define QUARTZ_MAX_BASE 16

typedef struct quartz_Buffer {
	quartz_Thread *Q;
	char *buf;
	size_t len;
	size_t cap;
} quartz_Buffer;

char *quartz_strdup(quartz_Thread *Q, const char *s);
void quartz_strfree(quartz_Thread *Q, char *s);

quartz_Errno quartz_bufinit(quartz_Thread *Q, quartz_Buffer *buf, size_t cap);
void quartz_bufdestroy(quartz_Buffer *buf);
char *quartz_bufreserve(quartz_Buffer *buf, size_t amount);
quartz_Errno quartz_bufputc(quartz_Buffer *buf, char c);
quartz_Errno quartz_bufputcr(quartz_Buffer *buf, char c, size_t amount);
quartz_Errno quartz_bufputs(quartz_Buffer *buf, const char *s);
quartz_Errno quartz_bufputls(quartz_Buffer *buf, const char *s, size_t len);
quartz_Errno quartz_bufputn(quartz_Buffer *buf, quartz_Int n);
quartz_Errno quartz_bufputnx(quartz_Buffer *buf, quartz_Int n, size_t base, size_t minDigits, bool zeroed);
size_t quartz_bufcount(quartz_Buffer *buf);
void quartz_bufcpy(quartz_Buffer *buf, char *s);
char *quartz_bufstr(quartz_Buffer *buf, size_t *len);
void quartz_bufshrinkto(quartz_Buffer *buf, size_t len);
void quartz_bufreset(quartz_Buffer *buf);

#endif
