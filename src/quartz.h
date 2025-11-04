#ifndef QUARTZ_H
#define QUARTZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

typedef intptr_t quartz_Int;
typedef uintptr_t quartz_Uint;
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

#ifndef QUARTZ_MAX_STACK
#define QUARTZ_MAX_STACK 1024
#endif

#ifndef QUARTZ_MAX_CALL
#define QUARTZ_MAX_CALL 64
#endif

typedef enum quartz_Errno {
	QUARTZ_OK = 0,
	// out of memory error. Error handler is not called for these.
	QUARTZ_ENOMEM,
	// I/O error.
	QUARTZ_EIO,
	// Stackoverflow.
	QUARTZ_ESTACK,
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
quartz_Errno quartz_bufputp(quartz_Buffer *buf, const void *p);
quartz_Errno quartz_bufputn(quartz_Buffer *buf, quartz_Int n);
quartz_Errno quartz_bufputnx(quartz_Buffer *buf, quartz_Int n, size_t base, size_t minDigits, bool zeroed);
quartz_Errno quartz_bufputu(quartz_Buffer *buf, quartz_Uint n);
quartz_Errno quartz_bufputux(quartz_Buffer *buf, quartz_Uint n, size_t base, size_t minDigits, bool zeroed);

// uses a non-standard format specifier syntax, which matches the one used
// by the Quartz language.
// It is similar to standard C formatting, except using Quartz values in most places.
// The format language has %% for normal %, and %, followed by non-alphabetic characters, then followed by 1 alphabetic character (no size specifiers).
// The non-alphabetic characters must be digits and ., for specifiers such as %5.3s
// The behavior of invalid format specifiers is undefined.
// Available formats are:
// - %p outputs the address of a pointer, and takes in a void *
// - %s outputs a C string, and takes in a const char *
// - %d outputs a base-10 signed integer, and takes in a quartz_Int.
// - %u outputs a base-10 unsigned integer, and takes in a quartz_Uint.
// - %x outputs a base-16 signed integer, and takes in a quartz_Int.
// - %X outputs a base-16 unsigned integer, and takes in a quartz_Uint.
// - %o outputs a base-8 signed integer, and takes in a quartz_Int.
// - %O outputs a base-8 unsigned integer, and takes in a quartz_Uint.
// - %b outputs a base-2 signed integer, and takes in a quartz_Int.
// - %B outputs a base-2 unsigned integer, and takes in a quartz_Uint.
// - %c outputs a character, and takes in an int.
// - %f outputs a base-10 real number, and takes in a quartz_Real.
// - %C outputs a base-10 complex number, and takes in a quartz_Complex.
quartz_Errno quartz_bufputf(quartz_Buffer *buf, const char *fmt, ...);

// see quartz_bufputf for format specifier details
quartz_Errno quartz_bufputfv(quartz_Buffer *buf, const char *fmt, va_list args);

size_t quartz_bufcount(quartz_Buffer *buf);
void quartz_bufcpy(quartz_Buffer *buf, char *s);
char *quartz_bufstr(quartz_Buffer *buf, size_t *len);
void quartz_bufshrinkto(quartz_Buffer *buf, size_t len);
void quartz_bufreset(quartz_Buffer *buf);

// clear error object
void quartz_clearerror(quartz_Thread *Q);
// pop value and error
quartz_Errno quartz_error(quartz_Thread *Q);
// raise formatted string
quartz_Errno quartz_errorfv(quartz_Thread *Q, const char *fmt, va_list args);
// raise formatted string
quartz_Errno quartz_errorf(quartz_Thread *Q, const char *fmt, ...);
// specialized function for OOMs
quartz_Errno quartz_oom(quartz_Thread *Q);
// generic function for errnos.
// If you error out as QUARTZ_ERUNTIME, it will simply error out a generic runtime error.
quartz_Errno quartz_erroras(quartz_Thread *Q, quartz_Errno err);

// get the index of the stack top. (stacksize - 1)
size_t quartz_gettop(quartz_Thread *Q);
// get the size of the current stack frame.
size_t quartz_gettop(quartz_Thread *Q);
// set the size of the current stack frame. 
quartz_Errno quartz_setstacksize(quartz_Thread *Q, size_t top);

// push data
quartz_Errno quartz_pushint(quartz_Thread *Q, quartz_Int i);
quartz_Errno quartz_pushreal(quartz_Thread *Q, quartz_Real r);
quartz_Errno quartz_pushcomplex(quartz_Thread *Q, quartz_Complex c);
quartz_Errno quartz_pushcomplexsum(quartz_Thread *Q, quartz_Real real, quartz_Real imaginary);
quartz_Errno quartz_pushstring(quartz_Thread *Q, const char *s);
quartz_Errno quartz_pushlstring(quartz_Thread *Q, const char *s, size_t len);
quartz_Errno quartz_pushcfunction(quartz_Thread *Q, quartz_CFunction f);
quartz_Errno quartz_pushmap(quartz_Thread *Q, size_t cap);
quartz_Errno quartz_pushlist(quartz_Thread *Q, size_t len, size_t cap);
quartz_Errno quartz_pushtuple(quartz_Thread *Q, size_t len);
quartz_Errno quartz_pushstruct(quartz_Thread *Q, size_t fields);
quartz_Errno quartz_pushregistry(quartz_Thread *Q);
quartz_Errno quartz_pushglobals(quartz_Thread *Q);
quartz_Errno quartz_pushvalue(quartz_Thread *Q, int idx);
// pop top value and push as result struct
quartz_Errno quartz_pushresult(quartz_Thread *Q, bool ok);
// push current error
quartz_Errno quartz_pusherror(quartz_Thread *Q);

// basic stack ops
void quartz_swap(quartz_Thread *Q, int a, int b);
void quartz_copy(quartz_Thread *Q, int from, int to);
void quartz_dup(quartz_Thread *Q);
void quartz_pop(quartz_Thread *Q);
void quartz_dupn(quartz_Thread *Q, size_t times);
void quartz_popn(quartz_Thread *Q, size_t times);

typedef enum quartz_CallFlags {
	QUARTZ_CALL_STATIC = 1<<0, // ret is not needed
	QUARTZ_CALL_PROTECTED = 1<<1, // a try call
} quartz_CallFlags;

// basic data ops
quartz_Type quartz_typeof(quartz_Thread *Q, int x);
const char *quartz_typenameof(quartz_Thread *Q, int x);
// pop key, then pop value, and push value[key]
quartz_Errno quartz_getIndex(quartz_Thread *Q);
// push cstring field then call quartz_getIndex()
quartz_Errno quartz_getField(quartz_Thread *Q, const char *field);
// pop the value and set the error handler. The error handler may be a function or null
// if null, normal error handling is used.
// In the case of OOM errors, the error handler is not called.
void quartz_seterrorhandler(quartz_Thread *Q);
quartz_Errno quartz_call(quartz_Thread *Q, size_t argc, quartz_CallFlags flags);

// getting data out
const char *quartz_tostring(quartz_Thread *Q, int x, quartz_Errno *err);
const char *quartz_tolstring(quartz_Thread *Q, int x, size_t *len, quartz_Errno *err);
quartz_Int quartz_tointeger(quartz_Thread *Q, int x, quartz_Errno *err);
quartz_Real quartz_toreal(quartz_Thread *Q, int x, quartz_Errno *err);
quartz_Complex quartz_tocomplex(quartz_Thread *Q, int x, quartz_Errno *err);

#endif
