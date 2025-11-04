#include "quartz.h"
#include "value.h"
#include "context.h"
#include "gc.h"

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
	gState->errorValue.type = QUARTZ_VNULL;
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
	Q->gState->errorValue.type = QUARTZ_VNULL;
}

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

quartz_Errno quartzI_invokeErrorHandler(quartz_Thread *Q) {
	return QUARTZ_OK;
}

quartz_Errno quartzI_ensureStackSize(quartz_Thread *Q, size_t size) {
	if(size > QUARTZ_MAX_STACK) {
		return QUARTZ_ESTACK;
	}
	return QUARTZ_OK;
}

quartz_Value quartzI_getStackValue(quartz_Thread *Q, int x);
void quartzI_setStackValue(quartz_Thread *Q, int x, quartz_Value v);
