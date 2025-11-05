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

	gState->registry = quartzI_newMap(Q);
	gState->globals = quartzI_newMap(Q);
	gState->loaded = quartzI_newMap(Q);
	gState->resultFields = quartzI_newTuple(Q, 2);

	gState->resultFields->vals[0].type = QUARTZ_VOBJ;
	gState->resultFields->vals[0].obj = &quartzI_newCString(Q, "value")->obj;
	gState->resultFields->vals[1].type = QUARTZ_VOBJ;
	gState->resultFields->vals[1].obj = &quartzI_newCString(Q, "error")->obj;

	gState->gcBlocked = false; // memory can be reclaimed now
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
		quartz_rawFree(&ctx, Q->stack, sizeof(quartz_StackEntry) * Q->stackCap);
		quartz_rawFree(&ctx, Q, sizeof(quartz_Thread));
		quartz_rawFree(&ctx, s, sizeof(quartz_GlobalState));
		return;
	}
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
quartz_Errno quartz_error(quartz_Thread *Q, quartz_Errno exit);

// raise formatted string
quartz_Errno quartz_errorfv(quartz_Thread *Q, quartz_Errno exit, const char *fmt, va_list args) {
	quartz_Buffer buf;
	quartz_bufinit(Q, &buf, 64);
	if(quartz_bufputfv(&buf, fmt, args)) {
		quartz_bufdestroy(&buf);
		return quartz_oom(Q);
	}
	quartz_String *s = quartzI_newString(Q, buf.len, NULL);
	if(s == NULL) {
		quartz_bufdestroy(&buf);
		return quartz_oom(Q);
	}
	quartzI_memcpy(s->buf, buf.buf, buf.len);
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

quartz_Value quartzI_getStackValue(quartz_Thread *Q, int x);
void quartzI_setStackValue(quartz_Thread *Q, int x, quartz_Value v);

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
