#include "quartz.h"
#include "utils.h"
#include "value.h"
#include "context.h"
#include "gc.h"
#include "state.h"

quartz_Thread *quartz_newThread(quartz_Context *ctx) {
	size_t defaultStackSize = 64;
	size_t defaultCallSize = 8;

	// TODO: handle OOMs

	quartz_GlobalState *gState = quartz_rawAlloc(ctx, sizeof(*gState));
	quartz_StackEntry *stack = quartz_rawAlloc(ctx, sizeof(*stack) * defaultStackSize);
	quartz_CallEntry *call = quartz_rawAlloc(ctx, sizeof(*call) * defaultCallSize);
	quartz_Thread *Q = quartz_rawAlloc(ctx, sizeof(*Q));
	Q->gState = gState;
	Q->stack = stack;
	Q->stackLen = 0;
	Q->stackCap = defaultStackSize;
	Q->call = call;
	Q->callLen = 0;
	Q->callCap = defaultCallSize;
	Q->resumedBy = NULL;
	Q->resuming = NULL;
	Q->errorValue.type = QUARTZ_VNULL;

	// init gState
	gState->gcBlocked = true; // unfortunate GCs could fuck us over, so we just block vro
	gState->context = *ctx;
	gState->mainThread = Q;
	gState->gcCount = sizeof(*gState) + sizeof(*stack) * defaultStackSize + sizeof(*Q);
	gState->gcPeak = gState->gcCount;
	gState->gcRatio = 2;
	gState->gcTarget = gState->gcCount * gState->gcRatio;
	gState->objList = NULL;
	gState->graySet = NULL;
	gState->tmpArrSize = 0;
	gState->oomValue.type = QUARTZ_VOBJ;
	gState->oomValue.obj = &quartzI_newCString(Q, "out of memory")->obj;

	gState->badErrorValue.type = QUARTZ_VOBJ;
	gState->badErrorValue.obj = &quartzI_newCString(Q, "bad error")->obj;

	gState->registry = quartzI_newMap(Q, 0);
	gState->globals = quartzI_newMap(Q, 32);
	gState->loaded = quartzI_newMap(Q, 16);
	gState->resultFields = quartzI_newTuple(Q, 2);

	gState->resultFields->vals[0].type = QUARTZ_VOBJ;
	gState->resultFields->vals[0].obj = &quartzI_newCString(Q, "value")->obj;
	gState->resultFields->vals[1].type = QUARTZ_VOBJ;
	gState->resultFields->vals[1].obj = &quartzI_newCString(Q, "error")->obj;
	
	// stdio is opened when opening fs libs
	for(size_t i = 0; i < QUARTZ_STDFILE_COUNT; i++) {
		gState->stdfiles[i] = NULL;
	}

	gState->gcBlocked = false; // memory can be reclaimed now

	// basic setup (no stdio)
	quartzI_emptyTemporaries(Q);
	return Q;
}

void quartz_destroyThread(quartz_Thread *Q) {
	if(Q == NULL) return;
	if(Q == Q->gState->mainThread) {
		// main thread is destroyed
		quartz_GlobalState *s = Q->gState;
		// bye bye heap
		quartz_Object *obj = s->objList;
		while(obj) {
			quartz_Object *cur = obj;
			obj = cur->nextObject;
			quartzI_freeObject(Q, cur);
		}
		quartz_Context ctx = s->context;
		quartz_rawFree(&ctx, Q->call, sizeof(quartz_CallEntry) * Q->callCap);
		quartz_rawFree(&ctx, Q->stack, sizeof(quartz_StackEntry) * Q->stackCap);
		quartz_rawFree(&ctx, Q, sizeof(quartz_Thread));
		quartz_rawFree(&ctx, s, sizeof(quartz_GlobalState));
		return;
	}
	quartz_free(Q, Q->call, sizeof(quartz_CallEntry) * Q->callCap);
	quartz_free(Q, Q->stack, sizeof(quartz_StackEntry) * Q->stackCap);
	quartz_free(Q, Q, sizeof(quartz_Thread));
}

static void quartz_recordPeak(quartz_Thread *Q) {
	if(Q->gState->gcCount > Q->gState->gcPeak) {
		Q->gState->gcPeak = Q->gState->gcCount;
	}
}

// These functions do use the context, but they may trigger GC to best use memory.
// Prefer using them in low-memory environments
void *quartz_alloc(quartz_Thread *Q, size_t size) {
	void *mem = quartz_rawAlloc(&Q->gState->context, size);
	if(mem == NULL) {
		quartz_gc(Q);
		mem = quartz_rawAlloc(&Q->gState->context, size);
	}
	if(mem != NULL) {
		Q->gState->gcCount += size;
		quartz_recordPeak(Q);
	}
	return mem;
}

void *quartz_realloc(quartz_Thread *Q, void *memory, size_t oldSize, size_t newSize) {
	if(newSize == 0) {
		quartz_free(Q, memory, oldSize);
		return NULL;
	}
	void *mem = quartz_rawRealloc(&Q->gState->context, memory, oldSize, newSize);
	if(mem == NULL) {
		quartz_gc(Q);
		mem = quartz_rawRealloc(&Q->gState->context, memory, oldSize, newSize);
	}
	if(mem != NULL) {
		Q->gState->gcCount -= oldSize;
		Q->gState->gcCount += newSize;
		quartz_recordPeak(Q);
	}
	return mem;
}
void quartz_free(quartz_Thread *Q, void *memory, size_t size) {
	if(memory == NULL) return;
	quartz_rawFree(&Q->gState->context, memory, size);
	Q->gState->gcCount -= size;
}

double quartz_clock(quartz_Thread *Q) {
	return quartz_rawClock(&Q->gState->context);
}

size_t quartz_time(quartz_Thread *Q) {
	return quartz_rawTime(&Q->gState->context);
}

void quartz_clearerror(quartz_Thread *Q) {
	Q->errorValue.type = QUARTZ_VNULL;
}

// checks if an error object is present
bool quartz_checkerror(quartz_Thread *Q) {
	return Q->errorValue.type != QUARTZ_VNULL;
}

// pop value and error
quartz_Errno quartz_error(quartz_Thread *Q, quartz_Errno exit) {
	if(quartz_getstacksize(Q) == 0) {
		Q->errorValue = Q->gState->badErrorValue;
		return QUARTZ_ERUNTIME;
	}
	quartz_Value v = quartzI_getStackValue(Q, -1);
	if(v.type == QUARTZ_VNULL) v = Q->gState->badErrorValue;
	Q->errorValue = v;
	quartz_Errno err = quartz_pop(Q);
	if(err) return err;
	return exit;
}

// raise formatted string
quartz_Errno quartz_errorfv(quartz_Thread *Q, quartz_Errno exit, const char *fmt, va_list args) {
	quartz_Buffer buf;
	quartz_bufinit(Q, &buf, 64);
	if(quartz_bufputfv(&buf, fmt, args)) {
		quartz_bufdestroy(&buf);
		return quartz_oom(Q);
	}
	quartz_String *s = quartzI_newString(Q, buf.len, buf.buf);
	if(s == NULL) {
		quartz_bufdestroy(&buf);
		return quartz_oom(Q);
	}
	Q->errorValue.type = QUARTZ_VOBJ;
	Q->errorValue.obj = &s->obj;
	quartz_bufdestroy(&buf);
	// error in error handler
	quartz_Errno err = quartzI_invokeErrorHandler(Q);
	if(err) return err;
	return exit;
}

// raise formatted string
quartz_Errno quartz_errorf(quartz_Thread *Q, quartz_Errno exit, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	quartz_Errno err = quartz_errorfv(Q, exit, fmt, args);
	va_end(args);
	return err;
}

// specialized function for OOMs
quartz_Errno quartz_oom(quartz_Thread *Q) {
	Q->errorValue = Q->gState->oomValue;
	return QUARTZ_ENOMEM;
}

// generic function for errnos.
// If you error out as QUARTZ_ERUNTIME, it will simply error out a generic runtime error.
quartz_Errno quartz_erroras(quartz_Thread *Q, quartz_Errno err) {
	if(err == QUARTZ_ENOMEM) {
		return quartz_oom(Q);
	} else if(err == QUARTZ_ESTACK) {
		return quartz_errorf(Q, QUARTZ_ESTACK, "stack overflow");
	} else if(err == QUARTZ_EIO) {
		return quartz_errorf(Q, QUARTZ_EIO, "io error");
	} else if(err == QUARTZ_ESYNTAX) {
		return quartz_errorf(Q, QUARTZ_ESYNTAX, "syntax error");
	}
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "runtime error");
}

quartz_Errno quartz_assertf(quartz_Thread *Q, bool condition, quartz_Errno exit, const char *fmt, ...) {
	if(condition) return QUARTZ_OK;
	va_list arg;
	va_start(arg, fmt);
	quartz_Errno err = quartz_errorfv(Q, exit, fmt, arg);
	va_end(arg);
	return err;
}

quartz_Errno quartzI_invokeErrorHandler(quartz_Thread *Q) {
	return QUARTZ_OK;
}

quartz_Errno quartzI_ensureStackSize(quartz_Thread *Q, size_t size) {
	if(size > QUARTZ_MAX_STACK) {
		return quartz_erroras(Q, QUARTZ_ESTACK);
	}
	size_t newCap = Q->stackCap;
	while(newCap < size) newCap *= 2;
	if(newCap != Q->stackCap) {
		quartz_StackEntry *newStack = quartz_realloc(Q, Q->stack, sizeof(*newStack) * Q->stackCap, sizeof(*newStack) * newCap);
		if(newStack == NULL) {
			return quartz_oom(Q);
		}
		Q->stack = newStack;
		Q->stackCap = newCap;
	}
	for(size_t i = Q->stackLen; i < size; i++) {
		Q->stack[i].isPtr = false;
		Q->stack[i].value.type = QUARTZ_VNULL;
	}
	Q->stackLen = size;
	return QUARTZ_OK;
}

quartz_Value quartzI_getStackValue(quartz_Thread *Q, int x) {
	size_t off = quartzI_stackFrameOffset(Q);
	size_t size = quartz_getstacksize(Q);

	quartz_Value null = {.type = QUARTZ_VNULL};
	if(x < 0) {
		x += size;
		if(x < 0) return null; // underflow
	}
	size_t i = off + x;
	if(i >= Q->stackLen) return null;
	quartz_StackEntry entry = Q->stack[i];
	if(entry.isPtr) {
		quartz_Pointer *p = (quartz_Pointer *)entry.value.obj;
		return p->val;
	}
	return entry.value;
}

quartz_Pointer *quartzI_getStackValuePointer(quartz_Thread *Q, int x, quartz_Errno *err) {
	size_t off = quartzI_stackFrameOffset(Q);
	size_t size = quartz_getstacksize(Q);
	*err = QUARTZ_OK;

	quartz_Value null = {.type = QUARTZ_VNULL};
	if(x < 0) {
		x += size;
		if(x < 0) {
			*err = quartz_errorf(Q, QUARTZ_ERUNTIME, "bad stack index: %d", (quartz_Int)x);
			return NULL; // underflow
		}
	}
	size_t i = off + x;
	if(i >= Q->stackLen) {
		*err = quartz_errorf(Q, QUARTZ_ERUNTIME, "bad stack index: %d", (quartz_Int)x);
		return NULL;
	}
	quartz_StackEntry entry = Q->stack[i];
	if(entry.isPtr) {
		quartz_Pointer *p = (quartz_Pointer *)entry.value.obj;
		return p;
	}
	quartz_Pointer *p = quartzI_newPointer(Q);
	if(p == NULL) {
		*err = quartz_oom(Q);
		return NULL;
	}
	p->val = entry.value;
	Q->stack[i] = (quartz_StackEntry) {
		.isPtr = true,
		.value.type = QUARTZ_VOBJ,
		.value.obj = &p->obj,
	};
	return p;
}

void quartzI_setStackValue(quartz_Thread *Q, int x, quartz_Value v) {
	size_t off = quartzI_stackFrameOffset(Q);
	size_t size = quartz_getstacksize(Q);

	if(x < 0) {
		x += size;
		if(x < 0) return; // underflow
	}
	size_t i = off + x;
	if(i >= Q->stackLen) return;
	quartz_StackEntry *entry = Q->stack + i;
	if(entry->isPtr) {
		quartz_Pointer *p = (quartz_Pointer *)entry->value.obj;
		p->val = v;
		return;
	}
	entry->value = v;
}

quartz_Errno quartzI_getFunctionIndex(quartz_Thread *Q, size_t *idx) {
	if(Q->callLen == 0) return quartz_errorf(Q, QUARTZ_ERUNTIME, "missing call entry");
	*idx = Q->call[Q->callLen-1].funcStackIdx;
	return QUARTZ_OK;
}

size_t quartzI_stackFrameOffset(quartz_Thread *Q) {
	if(Q->callLen == 0) return 0;
	quartz_CallEntry c = Q->call[Q->callLen-1];
	return c.funcStackIdx+1;
}

// get the index of the stack top. (stacksize - 1)
size_t quartz_gettop(quartz_Thread *Q) {
	return quartz_getstacksize(Q) - 1;
}

// get the size of the current stack frame.
size_t quartz_getstacksize(quartz_Thread *Q) {
	return Q->stackLen - quartzI_stackFrameOffset(Q);
}

// set the size of the current stack frame. 
quartz_Errno quartz_setstacksize(quartz_Thread *Q, size_t size) {
	return quartzI_ensureStackSize(Q, quartzI_stackFrameOffset(Q) + size);
}

bool quartz_validstackslot(quartz_Thread *Q, int x) {
	size_t off = quartzI_stackFrameOffset(Q);
	size_t size = quartz_getstacksize(Q);

	if(x < 0) {
		x += size;
		if(x < 0) return false; // underflow
	}
	size_t i = off + x;
	return i < Q->stackLen;
}

quartz_Errno quartzI_pushRawValue(quartz_Thread *Q, quartz_Value v) {
	quartz_Errno err = quartz_setstacksize(Q, quartz_getstacksize(Q)+1);
	if(err) return err;
	quartzI_setStackValue(Q, -1, v);
	return QUARTZ_OK;
}

quartz_Errno quartz_swap(quartz_Thread *Q, int a, int b) {
	if(!quartz_validstackslot(Q, a)) return quartz_errorf(Q, QUARTZ_ERUNTIME, "invalid swap operand #1: %d", (quartz_Int)a);
	if(!quartz_validstackslot(Q, b)) return quartz_errorf(Q, QUARTZ_ERUNTIME, "invalid swap operand #2: %d", (quartz_Int)b);
	quartz_Value tmp = quartzI_getStackValue(Q, a);
	quartzI_setStackValue(Q, a, quartzI_getStackValue(Q, b));
	quartzI_setStackValue(Q, b, tmp);
	return QUARTZ_OK;
}

quartz_Errno quartz_copy(quartz_Thread *Q, int from, int to) {
	if(!quartz_validstackslot(Q, from)) return quartz_errorf(Q, QUARTZ_ERUNTIME, "invalid copy source: %d", (quartz_Int)from);
	if(!quartz_validstackslot(Q, to)) return quartz_errorf(Q, QUARTZ_ERUNTIME, "invalid copy destination: %d", (quartz_Int)to);
	quartzI_setStackValue(Q, to, quartzI_getStackValue(Q, from));
	return QUARTZ_OK;
}

quartz_Errno quartz_dup(quartz_Thread *Q) {
	return quartz_dupn(Q, 1);
}

quartz_Errno quartz_pop(quartz_Thread *Q) {
	return quartz_popn(Q, 1);
}

quartz_Errno quartz_dupn(quartz_Thread *Q, size_t times) {
	for(size_t i = 0; i < times; i++) {
		quartz_Value v = quartzI_getStackValue(Q, -times);
		quartz_Errno err = quartzI_pushRawValue(Q, v);
		if(err) return err;
	}
	return QUARTZ_OK;
}

quartz_Errno quartz_popn(quartz_Thread *Q, size_t times) {
	size_t size = quartz_getstacksize(Q);
	if(size < times) return quartz_errorf(Q, QUARTZ_ERUNTIME, "stack underflow by %u", (quartz_Uint)(times - size));
	return quartz_setstacksize(Q, size - times);
}

quartz_Errno quartz_pushnull(quartz_Thread *Q) {
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VNULL});
}

quartz_Errno quartz_pushbool(quartz_Thread *Q, bool c) {
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VBOOL, .b = c});
}

quartz_Errno quartz_pushint(quartz_Thread *Q, quartz_Int i) {
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VINT, .integer = i});
}

quartz_Errno quartz_pushreal(quartz_Thread *Q, quartz_Real r) {
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VNUM, .real = r});
}

quartz_Errno quartz_pushcomplex(quartz_Thread *Q, quartz_Complex c) {
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VNUM, .complex = c});
}

quartz_Errno quartz_pushcomplexsum(quartz_Thread *Q, quartz_Real real, quartz_Real imaginary) {
	return quartz_pushcomplex(Q, (quartz_Complex) {.real = real, .imaginary = imaginary});
}

quartz_Errno quartz_pushstring(quartz_Thread *Q, const char *s) {
	return quartz_pushlstring(Q, s, quartzI_strlen(s));
}

quartz_Errno quartz_pushlstring(quartz_Thread *Q, const char *s, size_t len) {
	quartz_String *str = quartzI_newString(Q, len, s);
	if(str == NULL) return quartz_oom(Q);
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VOBJ, .obj = &str->obj});
}

quartz_Errno quartz_pushfstring(quartz_Thread *Q, const char *fmt, ...) {
	va_list arg;
	va_start(arg, fmt);
	quartz_Errno err = quartz_pushfstringv(Q, fmt, arg);
	va_end(arg);
	return err;
}

quartz_Errno quartz_pushfstringv(quartz_Thread *Q, const char *fmt, va_list arg) {
	quartz_Buffer buf;
	quartz_bufinit(Q, &buf, 64);
	if(quartz_bufputfv(&buf, fmt, arg)) {
		quartz_bufdestroy(&buf);
		return quartz_oom(Q);
	}
	quartz_String *s = quartzI_newString(Q, buf.len, buf.buf);
	if(s == NULL) {
		quartz_bufdestroy(&buf);
		return quartz_oom(Q);
	}
	quartz_bufdestroy(&buf);
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VOBJ, .obj = &s->obj});
}

quartz_Errno quartz_pushcfunction(quartz_Thread *Q, quartz_CFunction *f) {
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VCFUNC, .func = f});
}

static quartz_Errno quartzI_pushRawMap(quartz_Thread *Q, quartz_Map *map) {
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VOBJ, .obj = &map->obj});
}

quartz_Errno quartz_pushregistry(quartz_Thread *Q) {
	return quartzI_pushRawMap(Q, Q->gState->registry);
}

quartz_Errno quartz_pushglobals(quartz_Thread *Q) {
	return quartzI_pushRawMap(Q, Q->gState->globals);
}

quartz_Errno quartz_pushloaded(quartz_Thread *Q) {
	return quartzI_pushRawMap(Q, Q->gState->loaded);
}

quartz_Errno quartz_pushvalue(quartz_Thread *Q, int idx) {
	return quartzI_pushRawValue(Q, quartzI_getStackValue(Q, idx));
}

quartz_Errno quartz_pusherror(quartz_Thread *Q) {
	return quartzI_pushRawValue(Q, Q->errorValue);
}

quartz_Errno quartz_pushpointer(quartz_Thread *Q, int x) {
	quartz_Errno err;
	quartz_Pointer *p = quartzI_getStackValuePointer(Q, x, &err);
	if(err) return err;
	return quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VOBJ, .obj = &p->obj});
}

quartz_Type quartz_typeof(quartz_Thread *Q, int x) {
	return quartzI_trueTypeOf(quartzI_getStackValue(Q, x));
}

const char *quartz_typenameof(quartz_Thread *Q, int x) {
	return quartz_typenames[quartz_typeof(Q, x)];
}

quartz_Errno quartz_typeassert(quartz_Thread *Q, int x, quartz_Type expected) {
	quartz_Type actual = quartz_typeof(Q, x);
	if(actual != expected) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s expected, got %s", quartz_typenames[expected], quartz_typenames[actual]);
	}
	return QUARTZ_OK;
}

quartz_Errno quartz_stackassert(quartz_Thread *Q, size_t minStack) {
	size_t actual = quartz_getstacksize(Q);
	if(actual < minStack) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stack underflow by %u", (quartz_Uint)(minStack - actual));
	}
	return QUARTZ_OK;
}

quartz_Errno quartz_getindex(quartz_Thread *Q) {
	// we take first, then pop after, because of GC.
	quartz_Errno err;
	err = quartz_stackassert(Q, 2);
	if(err) return err;
	quartz_Value key = quartzI_getStackValue(Q, -1);
	quartz_Value value = quartzI_getStackValue(Q, -2);
	quartz_Value out;
	err = quartzI_getIndex(Q, value, key, &out);
	if(err) return err;
	quartzI_setStackValue(Q, -2, out);
	return quartz_pop(Q);
}

quartz_Errno quartz_setindex(quartz_Thread *Q) {
	quartz_Errno err;
	err = quartz_stackassert(Q, 3);
	if(err) return err;
	quartz_Value container = quartzI_getStackValue(Q, -3);
	quartz_Value key = quartzI_getStackValue(Q, -2);
	quartz_Value val = quartzI_getStackValue(Q, -1);

	err = quartzI_setIndex(Q, container, key, val);
	if(err) return err;
	return quartz_popn(Q, 3);
}

quartz_Errno quartz_loadPtr(quartz_Thread *Q, int ptr, int val) {
	quartz_Errno err;
	err = quartz_typeassert(Q, ptr, QUARTZ_TPOINTER);
	if(err) return err;
	quartz_Value p = quartzI_getStackValue(Q, ptr);
	quartz_Pointer *pPtr = (quartz_Pointer *)p.obj;
	quartzI_setStackValue(Q, val, pPtr->val);
	return QUARTZ_OK;
}

quartz_Errno quartz_storePtr(quartz_Thread *Q, int ptr, int val) {
	quartz_Errno err;
	err = quartz_typeassert(Q, ptr, QUARTZ_TPOINTER);
	if(err) return err;
	quartz_Value p = quartzI_getStackValue(Q, ptr);
	quartz_Pointer *pPtr = (quartz_Pointer *)p.obj;
	quartz_Value v = quartzI_getStackValue(Q, val);
	pPtr->val = v;
	return QUARTZ_OK;
}

const char *quartz_tostring(quartz_Thread *Q, int x, quartz_Errno *err) {
	return quartz_tolstring(Q, x, NULL, err);
}

const char *quartz_tolstring(quartz_Thread *Q, int x, size_t *len, quartz_Errno *err) {
	*err = quartz_typeassert(Q, x, QUARTZ_TSTR);
	if(*err) return NULL;
	quartz_Value v = quartzI_getStackValue(Q, x);
	quartz_String *s = (quartz_String *)v.obj;
	*err = QUARTZ_OK;
	if(len != NULL) *len = s->len;
	return s->buf;
}

quartz_Int quartz_tointeger(quartz_Thread *Q, int x, quartz_Errno *err) {
	*err = quartz_typeassert(Q, x, QUARTZ_TINT);
	if(*err) return 0;
	quartz_Value v = quartzI_getStackValue(Q, x);
	*err = QUARTZ_OK;
	return v.integer;
}

quartz_Real quartz_toreal(quartz_Thread *Q, int x, quartz_Errno *err) {
	*err = quartz_typeassert(Q, x, QUARTZ_TREAL);
	if(*err) return 0;
	quartz_Value v = quartzI_getStackValue(Q, x);
	*err = QUARTZ_OK;
	return v.real;
}

quartz_Complex quartz_tocomplex(quartz_Thread *Q, int x, quartz_Errno *err) {
	*err = quartz_typeassert(Q, x, QUARTZ_TCOMPLEX);
	if(*err) return (quartz_Complex) {0, 0};
	quartz_Value v = quartzI_getStackValue(Q, x);
	*err = QUARTZ_OK;
	return v.complex;
}

// get the length (for tuple, array, map and struct)
size_t quartz_len(quartz_Thread *Q, int x, quartz_Errno *err) {
	quartz_Value v = quartzI_getStackValue(Q, x);
	if(v.type == QUARTZ_VOBJ) {
		quartz_Object *o = v.obj;
		if(o->type == QUARTZ_OLIST) {
			quartz_List *l = (quartz_List *)o;
			return l->len;
		}
		if(o->type == QUARTZ_OSET) {
			quartz_Set *s = (quartz_Set *)o;
			return s->len;
		}
		if(o->type == QUARTZ_OTUPLE) {
			quartz_Tuple *t = (quartz_Tuple *)o;
			return t->len;
		}
		if(o->type == QUARTZ_OMAP) {
			quartz_Map *m = (quartz_Map *)o;
			// this isn't the amount of pairs alive
			// TODO: re-evaluate this behavior
			return m->filledAmount;
		}
	}
	*err = quartz_errorf(Q, QUARTZ_ERUNTIME, "container expected, got %s", quartz_typenameof(Q, x));
	return 0;
}

// get the capacity (for array and map)
size_t quartz_cap(quartz_Thread *Q, int x, quartz_Errno *err) {
	quartz_Value v = quartzI_getStackValue(Q, x);
	if(v.type == QUARTZ_VOBJ) {
		quartz_Object *o = v.obj;
		if(o->type == QUARTZ_OLIST) {
			quartz_List *l = (quartz_List *)o;
			return l->cap;
		}
		if(o->type == QUARTZ_OSET) {
			quartz_Set *s = (quartz_Set *)o;
			return s->cap;
		}
		if(o->type == QUARTZ_OTUPLE) {
			quartz_Tuple *t = (quartz_Tuple *)o;
			return t->len;
		}
		if(o->type == QUARTZ_OMAP) {
			quartz_Map *m = (quartz_Map *)o;
			return m->capacity;
		}
	}
	*err = quartz_errorf(Q, QUARTZ_ERUNTIME, "container expected, got %s", quartz_typenameof(Q, x));
	return 0;
}

size_t quartz_memsizeof(quartz_Thread *Q, int x) {
	return quartzI_memsizeof(quartzI_getStackValue(Q, x));
}
