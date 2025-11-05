#include "quartz.h"
#include "value.h"
#include "state.h"
#include "gc.h"

quartz_Errno quartzI_addCallEntry(quartz_Thread *Q, quartz_CallEntry *entry) {
	if(Q->callLen == QUARTZ_MAX_CALL) {
		return quartz_erroras(Q, QUARTZ_ESTACK);
	}
	if(Q->callLen == Q->callCap ) {
		size_t newCap = Q->callCap * 2;
		quartz_CallEntry *newStack = quartz_realloc(Q, Q->call, sizeof(*newStack) * Q->callCap, sizeof(*newStack) * newCap);
		if(newStack == NULL) {
			return quartz_oom(Q);
		}
		Q->call = newStack;
		Q->callCap = newCap;
	}
	Q->call[Q->callLen] = *entry;
	Q->callLen++;
	return QUARTZ_OK;
}

void quartzI_popCallEntry(quartz_Thread *Q) {
	if(Q->callLen > 0) Q->callLen--;
}

quartz_CallEntry *quartzI_getCallEntry(quartz_Thread *Q, size_t off) {
	if(Q->callLen <= off) {
		return NULL;
	}
	return Q->call + (Q->callLen - off - 1);
}

bool quartzI_isCurrentlyInterpreted(quartz_Thread *Q) {
	quartz_CallEntry *topEntry = quartzI_getCallEntry(Q, 0);
	if(topEntry == NULL) return NULL;
	return quartzI_isInterpretedFunction(topEntry->f);
}

quartz_Errno quartz_call(quartz_Thread *Q, size_t argc, quartz_CallFlags flags) {
	quartz_clearerror(Q);

	quartz_Errno err = QUARTZ_OK;

	if(quartz_getstacksize(Q) <= argc) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stack underflow");
	}

	bool wasFromInterpreter = quartzI_isCurrentlyInterpreted(Q);

	size_t funcIdx = Q->stackLen - argc - 1;
	err = quartz_typeassert(Q, -argc - 1, QUARTZ_TFUNCTION);
	if(err) return err;
	quartz_Value f = quartzI_getStackValue(Q, -argc - 1);
	quartz_CallEntry e = {
		.f = f,
		.funcStackIdx = funcIdx,
		.flags = flags,
	};
	if(quartzI_isInterpretedFunction(f)) {
		e.q.pc = 0;
		// technically can be left uninitialized but eh who cares
		e.q.funcSelected = 0;
	} else {
		e.c.k = NULL;
		e.c.ku = NULL;
	}
	err = quartzI_addCallEntry(Q, &e);
	if(err) return err;
	quartzI_setStackValue(Q, -argc - 1, (quartz_Value) {.type = QUARTZ_VNULL});
	if(!quartzI_isInterpretedFunction(f) || !wasFromInterpreter) {
		err = quartzI_runTopEntry(Q);
	}
	// error setup
	if(err && !quartz_checkerror(Q)) {
		return quartz_erroras(Q, err);
	}
	return err;
}

static quartz_Errno quartzI_callCFunc(quartz_Thread *Q, quartz_CFunction *f) {
	quartz_Errno err = f(Q, quartz_getstacksize(Q));
	// yielding is not handled yet so just always pop
	quartz_CallEntry *c = quartzI_getCallEntry(Q, 0);
	if(c->flags & QUARTZ_CALL_STATIC) {
		Q->stackLen = c->funcStackIdx;
	} else {
		Q->stackLen = c->funcStackIdx + 1;
	}
	quartzI_popCallEntry(Q);
	// API can make a bunch of garbage, so we should clean stuff
	quartzI_emptyTemporaries(Q);
	return err;
}

quartz_Errno quartzI_runTopEntry(quartz_Thread *Q) {
	quartz_CallEntry *e = quartzI_getCallEntry(Q, 0);
	if(e == NULL) return QUARTZ_OK; // don't ask
	if(quartzI_isInterpretedFunction(e->f)) {
		// TODO: specialized function
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "c functions only for now");
	}
	if(e->c.k != NULL) {
		// continuation
		return e->c.k(Q, QUARTZ_OK, e->c.ku);
	}
	quartz_Closure *c = quartzI_getClosure(e->f);
	if(c == NULL) {
		return quartzI_callCFunc(Q, e->f.func);
	}
	return quartzI_callCFunc(Q, c->f.func);
}
