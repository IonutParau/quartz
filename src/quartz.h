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

typedef enum quartz_FsAction {
	// open a file. buf should be cast to a string, it must be NULL terminated.
	// *buflen is a quartz_FileMode, and the file mode should be respected.
	// *fileData should be set to a pointer to the new file, whatever type it may be.
	// If QUARTZ_FILE_BINARY is not specified, the file should be open in an applicable text mode.
	QUARTZ_FS_OPEN,
	// should only change *fileData, and set it to a pointer to be used as the stdin/stdout/stderr.
	// buf points to a quartz_StdFile.
	// The file opened CAN STILL BE CLOSED, but should not be.
	QUARTZ_FS_STDFILE,
	// close a file. *fileData may be changed but doing so is pointless.
	QUARTZ_FS_CLOSE,
	// read a file. *fileData should not be changed. *buflen should be changed to signify how much data was read. The data should be put in buf.
	// 0 bytes read means EOF, though this may be changed
	QUARTZ_FS_READ,
	// write to a file. *fileData should not be changed. *buflen should be changed to signify how much data was written. The data is in buf.
	QUARTZ_FS_WRITE,
	// seek a file. *fileData should not be changed. *buflen should be set to the new cursor. buf points to a quartz_FsSeekArg.
	QUARTZ_FS_SEEK,
	// TODO: directory listing
} quartz_FsAction;

typedef enum quartz_FsSeekWhence {
	// set to off
	QUARTZ_SEEK_SET,
	// offset current by off
	QUARTZ_SEEK_CUR,
	// set to end - off.
	QUARTZ_SEEK_END,
} quartz_FsSeekWhence;

typedef struct quartz_FsSeekArg {
	quartz_FsSeekWhence whence;
	quartz_Int off;
} quartz_FsSeekArg;

typedef enum quartz_FileMode {
	QUARTZ_FILE_READABLE = 1<<0,
	QUARTZ_FILE_WRITABLE = 1<<1,
	QUARTZ_FILE_APPEND = 1<<2,
	QUARTZ_FILE_BINARY = 1<<3,
} quartz_FileMode;

typedef enum quartz_StdFile {
	QUARTZ_STDIN = 0,
	QUARTZ_STDOUT,
	QUARTZ_STDERR,

	// for internal purposes
	QUARTZ_STDFILE_COUNT,
} quartz_StdFile;

typedef enum quartz_FsBufMode {
	QUARTZ_FS_NOBUF,
	QUARTZ_FS_LINEBUF,
	QUARTZ_FS_FULLBUF,
} quartz_FsBufMode;

typedef quartz_Errno (*quartz_Filef)(quartz_Thread *Q, void *userdata, void **fileData, quartz_FsAction action, void *buf, size_t *buflen);

typedef struct quartz_File quartz_File;
typedef struct quartz_Context quartz_Context;

size_t quartz_sizeOfContext();
void quartz_initContext(quartz_Context *ctx, void *userdata);
void quartz_setAllocator(quartz_Context *ctx, quartz_Allocf alloc);
void quartz_setClock(quartz_Context *ctx, quartz_Clockf clock);
void quartz_setTime(quartz_Context *ctx, quartz_Timef time);
void quartz_setFileSystem(quartz_Context *ctx, quartz_Filef file, size_t defaultBufSize);
// configures how module resolution happens for things like loadlib.
// Path is the path list for scripts, cpath is the path list for native libraries
// All 3 can be set to NULL for default options.
// While the default search path may include various things, common paths are @.qrtz;@/lib.qrtz;lib/@.qrtz;lib/@/lib.qrtz
// pathConfig should be a string with 2 (tho may have less) characters.
// pathConfig[0] is treated as the file separator, meaning / is replaced with it after the path is computed.
// pathConfig[1] is treated as the symbol module separator, which is used when attempting to load dynamic libaries. Aka, when computing the symbol,
// it will load a symbol where the / in the module are replaced by this character. The default is _
void quartz_setModuleConfig(quartz_Context *ctx, const char *path, const char *cpath, const char *pathConfig);
const char *quartz_getModulePath(const quartz_Context *ctx);
const char *quartz_getModuleCPath(const quartz_Context *ctx);
const char *quartz_getModulePathConf(const quartz_Context *ctx);

quartz_Context *quartz_getContextOf(quartz_Thread *Q);

// For those who are confused, because we do not necessarily have a libc running
// we implement our own file abstraction layer and buffering.

quartz_File *quartz_fopen(quartz_Thread *Q, const char *path, size_t pathlen, quartz_FileMode mode, quartz_Errno *err);
quartz_Errno quartz_fclose(quartz_Thread *Q, quartz_File *f);
quartz_Errno quartz_fwrite(quartz_Thread *Q, quartz_File *f, const void *buf, size_t *buflen);
quartz_Errno quartz_fread(quartz_Thread *Q, quartz_File *f, void *buf, size_t *buflen);
quartz_Errno quartz_fseek(quartz_Thread *Q, quartz_File *f, quartz_FsSeekWhence whence, quartz_Int off, size_t *cursor);
quartz_Errno quartz_fflush(quartz_Thread *Q, quartz_File *f);
// if size is 0, the buffer size is unchanged
quartz_Errno quartz_fsetvbuf(quartz_Thread *Q, quartz_File *f, quartz_FsBufMode bufMode, size_t size);
quartz_File *quartz_fstdfile(quartz_Thread *Q, quartz_StdFile stdFile);
// f can be NULL, in which can fstdfile returns NULL as well
void quartz_fsetstdfile(quartz_Thread *Q, quartz_StdFile stdFile, quartz_File *f);
// open the stdio files and set them. This is not done automatically, as
// not all environments may have stdio.
quartz_Errno quartz_fopenstdio(quartz_Thread *Q);

typedef quartz_Errno (quartz_CFunction)(quartz_Thread *Q, size_t argc);
typedef quartz_Errno (quartz_KFunction)(quartz_Thread *Q, quartz_Errno state, void *context);

typedef enum quartz_UserOp {
	// An attempt to read a field. Arg 1 stores the field name as a string. Should return the value, or an error.
	QUARTZ_USER_GETFIELD,
	// An attempt to set a field. Arg 1 stores the field name as a string, and arg 2 stores the value.
	QUARTZ_USER_SETFIELD,
	// The userdata has been reclaimed by the garbage collector and thus the resources should be released. Associated values remain usable.
	// The GC does *not* support reviving currently, DO NOT ASSIGN IT TO A NEW VALUE.
	QUARTZ_USER_DESTROY,
} quartz_UserOp;

typedef quartz_Errno (quartz_UFunction)(quartz_Thread *Q, void *userdata, quartz_UserOp op);

// the stack size is in values
quartz_Thread *quartz_newThread(quartz_Context *ctx);
void quartz_destroyThread(quartz_Thread *Q);

// TODO: loading more of stdlib
quartz_Errno quartz_openstdlib(quartz_Thread *Q);
quartz_Errno quartz_openlibcore(quartz_Thread *Q);
quartz_Errno quartz_openlibio(quartz_Thread *Q);

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
// checks if an error object is present
bool quartz_checkerror(quartz_Thread *Q);
// pop value and error
quartz_Errno quartz_error(quartz_Thread *Q, quartz_Errno exit);
// raise formatted string
quartz_Errno quartz_errorfv(quartz_Thread *Q, quartz_Errno exit, const char *fmt, va_list args);
// raise formatted string
quartz_Errno quartz_errorf(quartz_Thread *Q, quartz_Errno exit, const char *fmt, ...);
// specialized function for OOMs
quartz_Errno quartz_oom(quartz_Thread *Q);
// generic function for errnos.
// If you error out as QUARTZ_ERUNTIME, it will simply error out a generic runtime error.
quartz_Errno quartz_erroras(quartz_Thread *Q, quartz_Errno err);
// error if condition is false
quartz_Errno quartz_assertf(quartz_Thread *Q, bool condition, quartz_Errno exit, const char *fmt, ...);

// loading libraries

typedef struct quartz_LibFunction {
	const char *name;
	quartz_CFunction *f;
} quartz_LibFunction;

// libs is terminated by NULL name.
// This function will push a map, and then set the corresponding fields to the appropriate C functions.
// It will also set the corresponding fields in loaded, so it can be imported.
quartz_Errno quartz_addlib(quartz_Thread *Q, const char *module, quartz_LibFunction *libs);
// just like addlib, except it also adds the global.
quartz_Errno quartz_addgloballib(quartz_Thread *Q, const char *global, quartz_LibFunction *libs);
// searches for a path the way import would.
// Pushes the result, with null being pushed if it couldn't be found.
quartz_Errno quartz_searchPath(quartz_Thread *Q, const char *module);
// will load a lib.
// This will try to push the result, but may error.
quartz_Errno quartz_import(quartz_Thread *Q, const char *module);

// get the index of the stack top. (stacksize - 1)
size_t quartz_gettop(quartz_Thread *Q);
// get the size of the current stack frame.
size_t quartz_getstacksize(quartz_Thread *Q);
// set the size of the current stack frame. 
quartz_Errno quartz_setstacksize(quartz_Thread *Q, size_t size);
bool quartz_validstackslot(quartz_Thread *Q, int x);

// push data
quartz_Errno quartz_pushnull(quartz_Thread *Q);
quartz_Errno quartz_pushbool(quartz_Thread *Q, bool c);
quartz_Errno quartz_pushint(quartz_Thread *Q, quartz_Int i);
quartz_Errno quartz_pushreal(quartz_Thread *Q, quartz_Real r);
quartz_Errno quartz_pushcomplex(quartz_Thread *Q, quartz_Complex c);
quartz_Errno quartz_pushcomplexsum(quartz_Thread *Q, quartz_Real real, quartz_Real imaginary);
quartz_Errno quartz_pushstring(quartz_Thread *Q, const char *s);
quartz_Errno quartz_pushlstring(quartz_Thread *Q, const char *s, size_t len);
quartz_Errno quartz_pushfstring(quartz_Thread *Q, const char *fmt, ...);
quartz_Errno quartz_pushfstringv(quartz_Thread *Q, const char *fmt, va_list arg);
quartz_Errno quartz_pushcfunction(quartz_Thread *Q, quartz_CFunction *f);
// uses the default globals
quartz_Errno quartz_pushscript(quartz_Thread *Q, const char *code, const char *src);
// uses the default globals
quartz_Errno quartz_pushlscript(quartz_Thread *Q, const char *code, size_t codelen, const char *src, size_t srclen);
// Pushes a Quartz function which executes the script.
// code, source, and globals are stack indexes to values. They should be a string, string and map.
quartz_Errno quartz_pushscriptx(quartz_Thread *Q, int code, int source, int globals);
quartz_Errno quartz_pushmap(quartz_Thread *Q);
quartz_Errno quartz_pushmapx(quartz_Thread *Q, size_t cap);
quartz_Errno quartz_pushlist(quartz_Thread *Q, size_t len);
quartz_Errno quartz_pushlistx(quartz_Thread *Q, size_t len, size_t cap);
quartz_Errno quartz_pushtuple(quartz_Thread *Q, size_t len);
quartz_Errno quartz_pushstruct(quartz_Thread *Q, size_t fields);
quartz_Errno quartz_pushregistry(quartz_Thread *Q);
quartz_Errno quartz_pushglobals(quartz_Thread *Q);
quartz_Errno quartz_pushloaded(quartz_Thread *Q);
quartz_Errno quartz_pushvalue(quartz_Thread *Q, int idx);
// pop top value and push as result struct
quartz_Errno quartz_pushresult(quartz_Thread *Q, bool ok);
// push current error
quartz_Errno quartz_pusherror(quartz_Thread *Q);
// push pointer to a value. All pointers to the value point to the same value.
quartz_Errno quartz_pushpointer(quartz_Thread *Q, int x);

// basic stack ops
quartz_Errno quartz_swap(quartz_Thread *Q, int a, int b);
quartz_Errno quartz_copy(quartz_Thread *Q, int from, int to);
quartz_Errno quartz_dup(quartz_Thread *Q);
quartz_Errno quartz_pop(quartz_Thread *Q);
quartz_Errno quartz_dupn(quartz_Thread *Q, size_t times);
quartz_Errno quartz_popn(quartz_Thread *Q, size_t times);

typedef enum quartz_CallFlags {
	QUARTZ_CALL_STATIC = 1<<0, // ret is not needed
	QUARTZ_CALL_PROTECTED = 1<<1, // a try call
} quartz_CallFlags;

typedef enum quartz_CmpFlags {
	QUARTZ_CMP_EQUAL = 1<<0,
	QUARTZ_CMP_LESS = 1<<1,
	QUARTZ_CMP_GREATER = 1<<2,
} quartz_CmpFlags;

// basic data ops
quartz_Type quartz_typeof(quartz_Thread *Q, int x);
const char *quartz_typenameof(quartz_Thread *Q, int x);
quartz_Errno quartz_typeassert(quartz_Thread *Q, int x, quartz_Type expected);
quartz_Errno quartz_stackassert(quartz_Thread *Q, size_t minStack);
// pop key, then pop value, and push value[key]
quartz_Errno quartz_getindex(quartz_Thread *Q);
// pop value, push value[field]
quartz_Errno quartz_getfield(quartz_Thread *Q, const char *field);
// pop value, push value[field]
quartz_Errno quartz_getlfield(quartz_Thread *Q, const char *field, size_t len);
// pop value, push value[idx]
quartz_Errno quartz_geti(quartz_Thread *Q, quartz_Uint idx);
// pop value, pop key, then pop container, do container[key] = value
quartz_Errno quartz_setindex(quartz_Thread *Q);
// pop value, pop container, container[field] = value
quartz_Errno quartz_setfield(quartz_Thread *Q, const char *field);
// pop value, pop container, container[field] = value
quartz_Errno quartz_setlfield(quartz_Thread *Q, const char *field, size_t len);
// pop value, pop container, container[idx] = value
quartz_Errno quartz_seti(quartz_Thread *Q, quartz_Uint idx);
// pop n values, then append them into the array at the new top.
// In the case of a tuple, it does not error. Instead, the tuple is overwritten with a new tuple, containing the new values.
// Can also be used on sets.
quartz_Errno quartz_append(quartz_Thread *Q, size_t n);
// on the stack should be (from top to bottom), value, key then container. Key of nil means start of the iteration.
// Key should be set to null at the start, to indicate the start of the iteration.
// The function will set key to the current key, and value to the current value of the iterator.
// This mechanism allows you to call next in a loop to iterate the entire container.
// If the next key is null, there is no more data left. Value is undefined in that scenario.
// In the case of:
// - arrays and tuples, the key is the index, and the value is the value at that index.
// - maps, the mapping is obvious.
// - structs, the key is the field, and the value is the value of the field.
quartz_Errno quartz_iterate(quartz_Thread *Q);
// pop the value and set the error handler. The error handler may be a function or null
// if null, normal error handling is used.
// In the case of OOM errors, the error handler is not called.
void quartz_seterrorhandler(quartz_Thread *Q);
quartz_Errno quartz_call(quartz_Thread *Q, size_t argc, quartz_CallFlags flags);
// return a value from a call
quartz_Errno quartz_return(quartz_Thread *Q, int x);
// set something to the current value of an upvalue
quartz_Errno quartz_loadUpval(quartz_Thread *Q, size_t upval, int x);
// store something in an upvalue
quartz_Errno quartz_storeUpval(quartz_Thread *Q, size_t upval, int x);
// set val to *ptr
quartz_Errno quartz_loadPtr(quartz_Thread *Q, int ptr, int val);
// set *ptr to val
quartz_Errno quartz_storePtr(quartz_Thread *Q, int ptr, int val);
bool quartz_contains(quartz_Thread *Q, int x, quartz_Errno *err);
quartz_CmpFlags quartz_compare(quartz_Thread *Q, int a, int b);

quartz_Errno quartz_setglobal(quartz_Thread *Q, const char *global, int x);
quartz_Errno quartz_getglobal(quartz_Thread *Q, const char *global);

quartz_Errno quartz_setloaded(quartz_Thread *Q, const char *mod, int x);
quartz_Errno quartz_getloaded(quartz_Thread *Q, const char *mod);

quartz_Errno quartz_setregistry(quartz_Thread *Q, const char *var, int x);
quartz_Errno quartz_getregistry(quartz_Thread *Q, const char *var);

// this will modify the value stored at x.
quartz_Errno quartz_cast(quartz_Thread *Q, int x, quartz_Type result);

// getting data out

const char *quartz_tostring(quartz_Thread *Q, int x, quartz_Errno *err);
const char *quartz_tolstring(quartz_Thread *Q, int x, size_t *len, quartz_Errno *err);
quartz_Int quartz_tointeger(quartz_Thread *Q, int x, quartz_Errno *err);
quartz_Real quartz_toreal(quartz_Thread *Q, int x, quartz_Errno *err);
quartz_Complex quartz_tocomplex(quartz_Thread *Q, int x, quartz_Errno *err);
bool quartz_truthy(quartz_Thread *Q, int x);
// get the length (for tuple, array, map and struct)
size_t quartz_len(quartz_Thread *Q, int x, quartz_Errno *err);
// get the capacity (for array and map)
size_t quartz_cap(quartz_Thread *Q, int x, quartz_Errno *err);
size_t quartz_memsizeof(quartz_Thread *Q, int x);

#endif
