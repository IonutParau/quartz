#ifndef QUARTZ_H
#define QUARTZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>

#define QRTZ_VER_MAJOR 0
#define QRTZ_VER_MINOR -1
#define QRTZ_VER_PATCH 0

#define QRTZ_VERSION "0.-1.0"

#ifndef QRTZ_STACKSIZE
#define QRTZ_STACKSIZE 16384
#endif

#ifndef QRTZ_CALLSTACKSIZE
#define QRTZ_CALLSTACKSIZE 128
#endif

#ifndef QRTZ_CHECKINTERVAL
#define QRTZ_CHECKINTERVAL 10000
#endif

// pass as a stack index to use other values
typedef enum qrtz_SpecialIndex {
	// registry map
	QRTZ_IDXREGISTRY = INT_MIN,
	// globals map
	QRTZ_IDXGLOBALS,
	// loaded map, a map of every module loaded to be cached
	QRTZ_IDXLOADED,
	// Result record
	QRTZ_IDXRESULT,
	// Ok record option
	QRTZ_IDXOK,
	// Error record option
	QRTZ_IDXERROR,
} qrtz_SpecialIndex;

// should, effectively, realloc(memory, newSize).
// If newSize is 0, free(memory).
// If memory is NULL, malloc(newSize).
// oldSize is passed in so bookkeeping need not necessarily be done.
typedef void *qrtz_Alloc(void *data, void *memory, size_t oldSize, size_t newSize);

typedef struct qrtz_Context {
	void *data;
	qrtz_Alloc *alloc;
} qrtz_Context;

typedef enum qrtz_Type {
	// null
	QRTZ_TNULL,
	// int
	QRTZ_TINT,
	// float
	QRTZ_TFLOAT,
	// bool
	QRTZ_TBOOL,
	// string
	QRTZ_TSTRING,
	// array
	QRTZ_TARRAY,
	// map
	QRTZ_TMAP,
	// a pointer
	QRTZ_TPOINTER,
	// a task
	QRTZ_TTASK,
	// program instance
	QRTZ_TPROGRAM,
	// generic user record
	QRTZ_TRECORD,
	// type
	QRTZ_TTYPE,
	// userdata
	QRTZ_TUSERDATA,
} qrtz_Type;

typedef struct qrtz_VM qrtz_VM;

typedef enum qrtz_Exit {
	// no error
	QRTZ_OK = 0,
	// out of memory
	QRTZ_ENOMEM,
	// stack overflow
	QRTZ_ENOSTACK,
	// io error
	QRTZ_EIO,
	// syntax error
	QRTZ_ESYNTAX,
	// runtime error
	QRTZ_ERUNTIME,
} qrtz_Exit;

typedef qrtz_Exit qrtz_CFunction(qrtz_VM *vm, size_t argc);

typedef enum qrtz_CallFlags {
	// Does not push return value
	QRTZ_CALL_STATIC = 1<<0,
	// Returns a Result and absorbs errors
	QRTZ_CALL_PROTECTED = 1<<1,
} qrtz_CallFlags;

typedef struct qrtz_Buffer {
	// The context of the buffer.
	// If NULL, then the buffer will
	// not be considered owned.
	// This means it'll never be freed.
	// This means if len == cap, it'll
	// return QRTZ_ENOMEM.
	qrtz_Context *ctx;
	// pointer to the buffer
	char *buf;
	// the length of the buffer
	size_t len;
	// the capacity of the buffer
	size_t cap;
} qrtz_Buffer;

// allocate an object
void *qrtz_calloc(qrtz_Context *ctx, size_t len);
// allocate an array of objects
void *qrtz_callocArray(qrtz_Context *ctx, size_t len, size_t count);
// free an object
void qrtz_cfree(qrtz_Context *ctx, void *memory, size_t len);
// free an array of objects
void qrtz_cfreeArray(qrtz_Context *ctx, void *memory, size_t len, size_t count);
// reallocate an array of objects
void *qrtz_crealloc(qrtz_Context *ctx, void *memory, size_t len, size_t oldCount, size_t newCount);

// allocate an object for a VM
void *qrtz_alloc(qrtz_VM *vm, size_t len);
// allocate an array of objects
void *qrtz_allocArray(qrtz_VM *vm, size_t len, size_t count);
// free an object
void qrtz_free(qrtz_VM *vm, void *memory, size_t len);
// free an array of objects
void qrtz_freeArray(qrtz_VM *vm, void *memory, size_t len, size_t count);
// reallocate an array of objects
void *qrtz_realloc(qrtz_VM *vm, void *memory, size_t len, size_t oldCount, size_t newCount);

// initializes a buffer as an empty buffer.
// This does 0 allocations.
void qrtz_initBuf(qrtz_Buffer *buf, qrtz_Context *ctx);
void qrtz_freeBuf(qrtz_Buffer *buf);

// initializes a buffer as an empty buffer.
// This does 1 allocation, which might fail.
qrtz_Exit qrtz_initBufCap(qrtz_Buffer *buf, qrtz_Context *ctx, size_t cap);

char *qrtz_reserveBuf(qrtz_Buffer *buf, size_t amount);
qrtz_Exit qrtz_putc(qrtz_Buffer *buf, char c);
qrtz_Exit qrtz_putcn(qrtz_Buffer *buf, char c, size_t rep);
qrtz_Exit qrtz_puts(qrtz_Buffer *buf, const char *s);
qrtz_Exit qrtz_putls(qrtz_Buffer *buf, const char *s, size_t len);
qrtz_Exit qrtz_putf(qrtz_Buffer *buf, const char *fmt, ...);
qrtz_Exit qrtz_vputf(qrtz_Buffer *buf, const char *fmt, va_list args);

// The size of a Quartz context.
// This can be used for ABI-agnostic code.
size_t qrtz_sizeofContext();

// load the default context
void qrtz_initContext(qrtz_Context *ctx);

// create a new instance of a VM, using a specific context.
qrtz_VM *qrtz_create(qrtz_Context *ctx);
// destroy a VM instance.
void qrtz_destroy(qrtz_VM *vm);
qrtz_Context *qrtz_contextOf(qrtz_VM *vm);

// basic API

// pushes an integer on the stack.
qrtz_Exit qrtz_pushint(qrtz_VM *vm, intptr_t n);
qrtz_Exit qrtz_pushnumber(qrtz_VM *vm, double n);
qrtz_Exit qrtz_pushstring(qrtz_VM *vm, const char *str);
qrtz_Exit qrtz_pushlstring(qrtz_VM *vm, const char *str, size_t len);
qrtz_Exit qrtz_pushglobal(qrtz_VM *vm, const char *name);
qrtz_Exit qrtz_pushcfunction(qrtz_VM *vm, qrtz_CFunction *f);
// pushes a C closure with a certain amount of upvalues available.
// It will load in pointers to the values as the upvalues, which can be loaded/set.
// The upvalues are simply the top [upvalues] values on the stack, and are popped afterwards.
qrtz_Exit qrtz_pushcclosure(qrtz_VM *vm, qrtz_CFunction *f, size_t upvalues);
// pushes a C closure with specific indexes as upvalues.
qrtz_Exit qrtz_pushcclosurex(qrtz_VM *vm, qrtz_CFunction *f, int *upvalIdxs, size_t upvalues);
// pushes a program. A program is an object 
qrtz_Exit qrtz_pushprogram(qrtz_VM *vm, const char *code, const char *name, int globals);
// pushes an array with a certain length and capacity.
// If capacity < len, the capacity will become len.
// If you do not care about the capacity, you can just pass 0.
// It will fill it with nulls.
qrtz_Exit qrtz_pusharray(qrtz_VM *vm, size_t len, size_t cap);
// Like qrtz_pusharray, but it will pop len values to make the array.
qrtz_Exit qrtz_pushstackarray(qrtz_VM *vm, size_t len, size_t cap);
// Similar to pushstackarray, but will push a map.
// It will pop len * 2 values, each of which being treated
// as key-value pairs of K1,V1,K2,V2,etc.
qrtz_Exit qrtz_pushmap(qrtz_VM *vm, size_t len, size_t cap);
// pushes the value at an index.
qrtz_Exit qrtz_pushvalue(qrtz_VM *vm, int idx);
// pushes a pointer to a value.
// This does not work on special indexes like QRTZ_IDXGLOBAL.
qrtz_Exit qrtz_pushpointer(qrtz_VM *vm, int idx);
// pushes userdata.
// type is copied.
// It will also pop [associated] values, storing them as an array
// of associated values.
qrtz_Exit qrtz_pushuserdata(qrtz_VM *vm, void *pointer, const char *type, size_t associated);
// pushes an instance of the Result record.
// isError determines if it is an instance of Ok, or an instance of Error.
qrtz_Exit qrtz_pushresult(qrtz_VM *vm, int val, bool isError);
// gets the true, logical type that a C API would be conserned with.
qrtz_Type qrtz_truetype(qrtz_VM *vm, int idx);
// does a semantic type-check against a type.
// The type can be loaded with something like pushglobal.
// For example:
// qrtz_pushglobal(vm, "String")
// assert(qrtz_istype(vm, -2, -1))
//
// As a reminder, there is no central Object record like in OOP languages.
// Do note that the records of built-ins are special-cased.
bool qrtz_istype(qrtz_VM *vm, int val, int ty);
bool qrtz_iserror(qrtz_VM *vm, int val);

// pushes the contents of an array.
qrtz_Exit qrtz_arraysplat(qrtz_VM *vm, int idx);

qrtz_Exit qrtz_call(qrtz_VM *vm, size_t argc, qrtz_CallFlags flags);
// call a function by index. The arguments all values after the function
qrtz_Exit qrtz_callit(qrtz_VM *vm, int idx, qrtz_CallFlags flags);

qrtz_Exit qrtz_pushupvaluepointer(qrtz_VM *vm, size_t userdataIdx);
qrtz_Exit qrtz_pushupvalue(qrtz_VM *vm, size_t userdataIdx);
qrtz_Exit qrtz_setupvalue(qrtz_VM *vm, size_t userdataIdx, int val);

// generic stack operatiosn

// returns the size of the current stack frame
size_t qrtz_getstacksize(qrtz_VM *vm);
// Sets the new stack size. If it grows, nulls are pushed.
qrtz_Exit qrtz_setstacksize(qrtz_VM *vm, size_t newStackSize);

// pops n values
qrtz_Exit qrtz_popn(qrtz_VM *vm, size_t n);
// pops 1 value
qrtz_Exit qrtz_pop(qrtz_VM *vm);
// dupes the top n values
qrtz_Exit qrtz_dupen(qrtz_VM *vm, size_t n);
// dupes the top value. Equivalent to qrtz_pushvalue(vm, -1)
qrtz_Exit qrtz_dupe(qrtz_VM *vm);
// swap 2 values
qrtz_Exit qrtz_swap(qrtz_VM *vm, int a, int b);
// rotate the top n values amount to the left (decreasing index)
qrtz_Exit qrtz_rotateleft(qrtz_VM *vm, size_t n, size_t amount);
// rotate the top n values amount to the right (increasing index)
qrtz_Exit qrtz_rotateright(qrtz_VM *vm, size_t n, size_t amount);

// set a value as the return value
qrtz_Exit qrtz_return(qrtz_VM *vm, int idx);

qrtz_Exit qrtz_getKey(qrtz_VM *vm, int container, int key);
qrtz_Exit qrtz_setKey(qrtz_VM *vm, int container, int key, int val);
qrtz_Exit qrtz_getField(qrtz_VM *vm, int container, const char *name);
qrtz_Exit qrtz_setField(qrtz_VM *vm, int container, const char *name, int val);
qrtz_Exit qrtz_getIndex(qrtz_VM *vm, int container, size_t idx);
qrtz_Exit qrtz_setIndex(qrtz_VM *vm, int container, size_t idx, int val);

// Getting data out

intptr_t qrtz_tointeger(qrtz_VM *vm, int idx, qrtz_Exit *err);
double qrtz_tonumber(qrtz_VM *vm, int idx, qrtz_Exit *err);
const char *qrtz_tostring(qrtz_VM *vm, int idx, qrtz_Exit *err);
const char *qrtz_tolstring(qrtz_VM *vm, int idx, size_t *len, qrtz_Exit *err);
size_t qrtz_lenof(qrtz_VM *vm, int idx, qrtz_Exit *err);
size_t qrtz_capof(qrtz_VM *vm, int idx, qrtz_Exit *err);
// iterates over an array, map, program, record or record type.
// It will return whether the iteration stopped, at which point nothing is done.
// If an interation succeeded, it will push the current value.
// internal should be initialized to 0, and is used to keep track of iteration.
bool qrtz_iter(qrtz_VM *vm, int idx, size_t *internal, qrtz_Exit *err);

// Loading the standard library

typedef struct qrtz_LibraryField {
	// NULL terminates array
	const char *name;
	qrtz_Type type;
	union {
		intptr_t integer;
		double number;
		bool boolean;
		const char *cstr;
		struct {
			char *buf;
			size_t len;
		} str;
		qrtz_CFunction *func;
	};
} qrtz_LibraryField;

// Defines a module which can be imported.
// The module is a map, not a program.
qrtz_Exit qrtz_defineModule(qrtz_VM *vm, const char *name, qrtz_LibraryField fields[]);
// Defines a module and writes its fields as globals as well.
// Pretty much only useful for openstdlib.
qrtz_Exit qrtz_defineGlobals(qrtz_VM *vm, const char *name, qrtz_LibraryField fields[]);

// Loads the entire standard library
qrtz_Exit qrtz_loadlibs(qrtz_VM *vm);

// Loads the stdlib module, defining base globals
qrtz_Exit qrtz_openstdlib(qrtz_VM *vm);
// Loads the stdio module
qrtz_Exit qrtz_openstdio(qrtz_VM *vm);
// Loads the tasks module
qrtz_Exit qrtz_opentasks(qrtz_VM *vm);
// Loads the strings module
qrtz_Exit qrtz_openstrings(qrtz_VM *vm);
// Loads the math module. Does nothing if QUARTZ_NOLIBM is defined.
qrtz_Exit qrtz_openmath(qrtz_VM *vm);
// Loads the buffers module.
qrtz_Exit qrtz_openbuffers(qrtz_VM *vm);

// Memory-related stuff

// returns the amount of memory owned by this value
// this is O(1), as it is not recursive
size_t qrtz_memsizeof(qrtz_VM *vm, int val);
// run a full stop-the-world GC cycle
void qrtz_gc(qrtz_VM *vm);
// gets the amount of bytes allocated by the VM.
size_t qrtz_getMemoryUsage(qrtz_VM *vm);
// Gets the current memory target.
// Once the memory usage exceeds the target, a GC cycle starts.
size_t qrtz_getMemoryTarget(qrtz_VM *vm);
// Sets the memory target.
void qrtz_setMemoryTarget(qrtz_VM *vm, size_t target);
// sets the ratio for determining new memory targets.
// Effectively, the formula is
// target = current * (1 + pause / 100).
// This means a pause of 100 will wait for memory usage to double.
void qrtz_setGCPause(qrtz_VM *vm, double pause);

#endif
