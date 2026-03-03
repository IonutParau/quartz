#ifndef QUARTZ_VAL
#define QUARTZ_VAL

#include <stdint.h>
#include <stddef.h>
#include "quartz.h"

typedef enum qrtz_ValTag : char {
	// an illegal value tag
	QRTZ_VILLEGAL = -1,
	// null
	QRTZ_VNULL = 0,
	// int
	QRTZ_VINT,
	// number
	QRTZ_VNUMBER,
	// c function
	QRTZ_VCFUNC,
	// pointer to object
	QRTZ_VOBJ,
} qrtz_ValTag;

typedef enum qrtz_ObjTag : char {
	QRTZ_OSTR,
	QRTZ_OARRAY,
	QRTZ_OMAP,
	QRTZ_OPOINTER,
	QRTZ_OUSERDATA,
	QRTZ_OTASK,
	QRTZ_OPROGRAM,
	QRTZ_ORECORD,
	QRTZ_ORECTYPE,
	QRTZ_ORECOPT,
	QRTZ_OFUNCTION,
	QRTZ_OCLOSURE,
} qrtz_ObjTag;

typedef struct qrtz_Value {
	qrtz_ValTag tag;
	union {
		bool boolean;
		intptr_t integer;
		double number;
		qrtz_CFunction *cfunc;
		struct qrtz_Object *object;
	};
} qrtz_Value;

typedef struct qrtz_Object {
	struct qrtz_Object *next;
	struct qrtz_Object *nextGray;
	qrtz_ObjTag tag;
	bool marked;
} qrtz_Object;

typedef struct qrtz_String {
	qrtz_Object obj;
	// cached string hash
	size_t hash;
	size_t len;
	char data[];
} qrtz_String;

typedef struct qrtz_Array {
	qrtz_Object obj;
	size_t len;
	size_t cap;
	qrtz_Value values[];
} qrtz_Array;

typedef struct qrtz_Map {
	qrtz_Object obj;
	size_t used;
	size_t cap;
	// separate from used, just to keep len() O(1)
	size_t len;
	// There first is an array of keys, then an array of values.
	// They are cap apart.
	// A key being set to VILLEGAL means unallocated.
	// A value being set to VILLEGAL means allocated but removed.
	qrtz_Value *data;
} qrtz_Map;

typedef struct qrtz_Pointer {
	qrtz_Object obj;
	qrtz_Value val;
} qrtz_Pointer;

typedef struct qrtz_Userdata {
	qrtz_Object obj;
	char *typestr;
	void *pointer;
	size_t associatedLen;
	qrtz_Value associated[];
} qrtz_Userdata;

typedef struct qrtz_CallEntry {
	int stacktop;
	bool isC;
} qrtz_CallEntry;

typedef struct qrtz_Task {
	qrtz_Object obj;
	size_t stacklen;
	// limited to QRTZ_STACKSIZE
	size_t stackcap;
	size_t calllen;
	// limited to QRTZ_CALLSTACKSIZE
	size_t callcap;
	// illegal values are pointers to the values
	qrtz_Value *stack;
	qrtz_CallEntry *calls;
	// the one we are waiting for. NULL means we are not waiting for anyone.
	struct qrtz_Task *waitingFor;
	// the one waiting for us. NULL means no one is waiting for us.
	struct qrtz_Task *waitedBy;
	double deadline;
	size_t checkcounter;
	// if 0, deadlines are disabled
	size_t checkinterval;
} qrtz_Task;

typedef struct qrtz_ProgramEntry {
	// a NULL name is reserved for the initialization logic
	char *name;
	size_t len;
	qrtz_Value val;
} qrtz_ProgramEntry;

typedef struct qrtz_Program {
	qrtz_Object obj;
	qrtz_Map *globals;
	qrtz_String *name;
	size_t entryUsed;
	size_t entryCap;
	qrtz_ProgramEntry *entries;
	size_t localCount;
	// Similar to stack entries, VILLEGAL means pointer to value.
	// Constants are stored as magic locals.
	qrtz_Value *locals;
} qrtz_Program;

typedef struct qrtz_Instruction {
	unsigned char op;
	unsigned char uA;
	union {
		struct {
			unsigned char B;
			unsigned char C;
		};
		unsigned short uD;
		short sD;
	};
} qrtz_Instruction;

typedef struct qrtz_Function {
	qrtz_Object obj;
	qrtz_Program *program;
	size_t codesize;
	qrtz_Instruction *inst;
} qrtz_Function;

typedef struct qrtz_Closure {
	qrtz_Object obj;
	qrtz_Value func;
	size_t upvalCount;
	qrtz_Pointer *upvals[];
} qrtz_Closure;

typedef struct qrtz_VM {
	// context we care about
	qrtz_Context ctx;
	// every object there is
	qrtz_Object *heap;
	// gray set, stuff the GC is currently marking
	qrtz_Object *graySet;
	size_t memUsage;
	size_t memTarget;
	double gcPause;
	qrtz_Map *globals;
	qrtz_Map *registry;
	qrtz_Map *loaded;
	// the main task. Cannot suspend, but can wait for others.
	qrtz_Task *mainTask;
	// Current task. Shortcut to not have to traverse the waitingFor chain every time.
	qrtz_Task *curTask;
} qrtz_VM;

qrtz_Object *qrtz_allocObject(qrtz_VM *vm, qrtz_ObjTag tag, size_t objSize);
qrtz_String *qrtz_allocStringObject(qrtz_VM *vm, const char *data, size_t len);
qrtz_Array *qrtz_allocArrayObject(qrtz_VM *vm, size_t cap);
qrtz_Map *qrtz_allocMapObject(qrtz_VM *vm, size_t cap);
qrtz_Pointer *qrtz_allocPointerObject(qrtz_VM *vm);
qrtz_Task *qrtz_allocTaskObject(qrtz_VM *vm, qrtz_Map *globals);

size_t qrtz_objmemsizeof(qrtz_Object *obj);
size_t qrtz_strhash(const char *s, size_t len);
size_t qrtz_valhash(qrtz_Value val);

#endif
