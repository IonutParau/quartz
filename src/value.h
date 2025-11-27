#ifndef QUARTZ_VALUE
#define QUARTZ_VALUE

#include "quartz.h"
#include "context.h"

typedef enum quartz_VType {
	QUARTZ_VNULL = 0,
	QUARTZ_VBOOL,
	QUARTZ_VINT,
	QUARTZ_VNUM,
	QUARTZ_VCOMPLEX,
	QUARTZ_VOBJ,
	QUARTZ_VCFUNC,
} quartz_VType;

typedef enum quartz_ObjectType {
	QUARTZ_OSTR,
	QUARTZ_OLIST,
	QUARTZ_OTUPLE,
	QUARTZ_OSET,
	QUARTZ_OMAP,
	QUARTZ_OSTRUCT,
	QUARTZ_OFUNCTION,
	QUARTZ_OCLOSURE,
	QUARTZ_OPOINTER,
	QUARTZ_OTHREAD,
	QUARTZ_OUSERDATA,
} quartz_ObjectType;

typedef enum quartz_ObjectFlags {
	QUARTZ_FLAG_MARKED = 1<<0,
	QUARTZ_FLAG_GRAY = 1<<1,
} quartz_ObjectFlags;

typedef struct quartz_Object {
	quartz_ObjectType type;
	quartz_ObjectFlags flags;
	struct quartz_Object *nextObject;
	struct quartz_Object *nextGray;
} quartz_Object;

typedef struct quartz_Value {
	quartz_VType type;
	union {
		bool b;
		quartz_Int integer;
		quartz_Real real;
		quartz_Complex complex;
		quartz_Object *obj;
		quartz_CFunction *func;
	};
} quartz_Value;

typedef struct quartz_String {
	quartz_Object obj;
	size_t hash;
	size_t len;
	char buf[];
} quartz_String;

typedef struct quartz_List {
	quartz_Object obj;
	size_t len;
	size_t cap;
	quartz_Value *vals;
} quartz_List;

typedef struct quartz_Tuple {
	quartz_Object obj;
	size_t hash;
	size_t len;
	quartz_Value vals[];
} quartz_Tuple;

typedef struct quartz_List quartz_Set;

typedef struct quartz_MapPair {
	// if this is null, slot is unallocated
	quartz_Value key;
	// if this is null, slot was used but then was closed
	quartz_Value val;
} quartz_MapPair;

// single-array hashmap
typedef struct quartz_Map {
	quartz_Object obj;
	size_t filledAmount;
	size_t capacity;
	quartz_MapPair *pairs;
} quartz_Map;

typedef struct quartz_Struct {
	quartz_Object obj;
	// SHOULD BE TUPLE OF STRINGS!!!!!
	quartz_Tuple *fields;
	// all the keys are meant to be *strings*
	quartz_Value pairs[];
} quartz_Struct;

typedef struct quartz_Upvalue {
	bool inStack;
	unsigned short stackIndex;
} quartz_Upvalue;

typedef enum quartz_FunctionFlags {
	QUARTZ_FUNC_CHUNK = 1<<0,
	QUARTZ_FUNC_VARARGS = 1<<1,
} quartz_FunctionFlags;

typedef struct quartz_Instruction {
	unsigned char op;
	unsigned char A;
	union {
		unsigned short uD;
		short sD;
		struct {
			unsigned char B;
			unsigned char C;
		};
	};
	unsigned int line;
} quartz_Instruction;

typedef struct quartz_Function {
	quartz_Object obj;
	// ordered weirdly to ensure most useful data is in cache
	size_t codesize;
	quartz_Instruction *code;
	size_t constCount;
	quartz_Value *consts;
	size_t upvalCount;
	quartz_Upvalue *upvaldefs;
	int flags;
	int argc;
	quartz_Map *module;
	quartz_Map *globals;
	quartz_String *chunkname;
	// how much to preallocate
	size_t stacksize;
} quartz_Function;

typedef struct quartz_Closure {
	quartz_Object obj;
	size_t len;
	quartz_Value f;
	quartz_Value ups[];
} quartz_Closure;

typedef struct quartz_Pointer {
	quartz_Object obj;
	quartz_Value val;
} quartz_Pointer;

// improbably high amount of objects
// TODO: use a dynamic array or linked list instead
#define QUARTZ_MAX_TMP 256

typedef struct quartz_GlobalState {
	quartz_Context context;
	quartz_Map *registry;
	quartz_Map *globals;
	quartz_Map *loaded;
	quartz_Thread *mainThread;
	quartz_Tuple *resultFields;
	quartz_Object *objList;
	quartz_Object *graySet;
	size_t gcCount;
	size_t gcTarget;
	size_t gcPeak;
	double gcRatio;
	// if you try to raise a null runtime error
	quartz_Value badErrorValue;
	// the error object used when we run out of memory (preallocated, obviously)
	quartz_Value oomValue;
	quartz_File *stdfiles[QUARTZ_STDFILE_COUNT];
	bool gcBlocked;
	quartz_Object *tmpArr[QUARTZ_MAX_TMP];
	size_t tmpArrSize;
} quartz_GlobalState;

typedef struct quartz_StackEntry {
	// if isPtr, this is an upvalue, and the value is a pointer.
	// if not, then this is an actual stack value (the common case), and thus
	// this is value.
	bool isPtr;
	quartz_Value value;
} quartz_StackEntry;

typedef struct quartz_CallEntry {
	quartz_Value f;
	// error handler
	quartz_Value errorh;
	// stack index of function, as in, what stack index to use for calls
	size_t funcStackIdx;
	quartz_CallFlags flags;
	union {
		// for c functions
		struct {
			quartz_KFunction *k;
			void *ku;
		} c;
		// for Quartz functions
		struct {
			quartz_Instruction *pc;
		} q;
	};
} quartz_CallEntry;

typedef struct quartz_Thread {
	quartz_Object obj;
	quartz_GlobalState *gState;
	// if null, no error value
	quartz_Value errorValue;
	// global error handler
	quartz_Value errorh;
	size_t stackCap;
	size_t stackLen;
	quartz_StackEntry *stack;
	size_t callCap;
	size_t callLen;
	quartz_CallEntry *call;
	struct quartz_Thread *resumedBy;
	struct quartz_Thread *resuming;
} quartz_Thread;

typedef struct quartz_Userdata {
	quartz_Object obj;
	void *userdata;
	size_t userdataSize;
	const char *typestr;
	quartz_UFunction *uFunc;
	size_t associatedLen;
	quartz_Value associated[];
} quartz_Userdata;

bool quartzI_validKey(quartz_Value val);
// always bake tuple hashes do avoid stackoverflow exploits!
size_t quartzI_hash(quartz_Value val);
size_t quartzI_memsizeof(quartz_Value val);
quartz_Type quartzI_trueTypeOf(quartz_Value v);
quartz_CmpFlags quartzI_compare(quartz_Value a, quartz_Value b);
bool quartzI_isInterpretedFunction(quartz_Value f);
quartz_Closure *quartzI_getClosure(quartz_Value f);
quartz_Function *quartzI_getFunction(quartz_Value f);
quartz_CFunction *quartzI_getCFunction(quartz_Value f);
quartz_Value quartzI_mapGet(quartz_Map *m, quartz_Value key);
bool quartzI_truthyValue(quartz_Value v);
bool quartzI_isLegalPair(quartz_MapPair pair);
quartz_Errno quartzI_mapSet(quartz_Thread *Q, quartz_Map *m, quartz_Value key, quartz_Value v);
quartz_Errno quartzI_getIndex(quartz_Thread *Q, quartz_Value container, quartz_Value key, quartz_Value *val);
quartz_Errno quartzI_setIndex(quartz_Thread *Q, quartz_Value container, quartz_Value key, quartz_Value val);
quartz_String *quartzI_valueToString(quartz_Thread *Q, quartz_Value v);
quartz_String *quartzI_valueQuoted(quartz_Thread *Q, quartz_Value v);

#endif
