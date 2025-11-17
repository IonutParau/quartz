#include "ops.h"
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
		quartz_Function *q = quartzI_getFunction(f);
		e.q.pc = q->code;
	} else {
		e.c.k = NULL;
		e.c.ku = NULL;
	}
	err = quartzI_addCallEntry(Q, &e);
	if(err) return err;
	// breaks off links
	Q->stack[funcIdx] = (quartz_StackEntry) {
		.isPtr = false,
		.value.type = QUARTZ_VNULL,
	};
	// TODO: make quartzI_vmExec smart enough to not recurse when calling other interpreted functions
	err = quartzI_runTopEntry(Q);
	// error setup
	if(err && !quartz_checkerror(Q)) {
		return quartz_erroras(Q, err);
	}
	return err;
}

quartz_Errno quartz_return(quartz_Thread *Q, int x) {
	quartz_Value v = quartzI_getStackValue(Q, x);
	quartz_CallEntry *e = quartzI_getCallEntry(Q, 0);
	if(e == NULL) return quartz_errorf(Q, QUARTZ_ERUNTIME, "missing call frame");
	// breaks off any link too
	Q->stack[e->funcStackIdx] = (quartz_StackEntry) {
		.isPtr = false,
		.value = v,
	};
	return QUARTZ_OK;
}

static quartz_Errno quartz_vmExec(quartz_Thread *Q) {
	quartz_CallEntry *c = quartzI_getCallEntry(Q, 0);
	if(!c) return QUARTZ_OK;
	quartz_Errno err = QUARTZ_OK;

	quartz_Instruction *pc = c->q.pc;
	quartz_Function *f = quartzI_getFunction(c->f);
	quartz_Closure *closure = quartzI_getClosure(c->f);
	while(quartz_getstacksize(Q) < f->argc) {
		err = quartz_pushnull(Q);
		if(err) return err;
	}
	while(true) {
		quartz_cleartmp(Q);
		// for anyone reading, this means if memory usage
		// is above the intended threshhold, do a GC.
		// Not that we do a GC on EVERY INSTRUCTION, that'd be devastating.
		quartzI_trygc(Q);
		if(pc->op == QUARTZ_OP_NOP) {
			// nothing
		} else if(pc->op == QUARTZ_OP_RETMOD) {
			Q->stack[c->funcStackIdx] = (quartz_StackEntry) {
				.isPtr = false,
				.value.type = QUARTZ_VOBJ,
				.value.obj = &f->module->obj,
			};
			goto done;
		} else if(pc->op == QUARTZ_OP_PUSHINT) {
			err = quartz_pushint(Q, pc->sD);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_PUSHCONST) {
			err = quartzI_pushRawValue(Q, f->consts[pc->uD]);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_GETEXTERN) {
			quartz_Value key = f->consts[pc->uD];
			quartz_Value v = quartzI_mapGet(f->module, key);
			if(v.type == QUARTZ_VNULL) {
				err = quartzI_pushRawValue(Q, quartzI_mapGet(f->globals, key));
				if(err) goto done;
			} else {
				err = quartzI_pushRawValue(Q, v);
				if(err) goto done;
			}
		} else if(pc->op == QUARTZ_OP_LOAD) {
			err = quartz_pushvalue(Q, pc->uD);
			if(err) return err;
		} else if(pc->op == QUARTZ_OP_CALL) {
			err = quartz_call(Q, pc->B, pc->C);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_GETFIELD) {
			err = quartz_getindex(Q);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_GETCONSTFIELD) {
			quartz_Value v = f->consts[pc->uD];
			err = quartzI_pushRawValue(Q, v);
			if(err) goto done;
			err = quartz_getindex(Q);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_VARARGPREP) {
			size_t argc = quartz_getstacksize(Q);
			err = quartz_pushtuple(Q, argc - pc->uD);
			if(err) goto done;
		} else {
			err = quartz_errorf(Q, QUARTZ_ERUNTIME, "bad opcode: %u", (quartz_Uint)pc->op);
			goto done;
		}
		pc++;
	}

done:
	c->q.pc = pc; // useless until we figure out yielding
	// no yielding yet, so always pop frame
	if(c->flags & QUARTZ_CALL_STATIC) {
		Q->stackLen = c->funcStackIdx;
	} else {
		Q->stackLen = c->funcStackIdx + 1;
	}
	quartzI_popCallEntry(Q);
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
	quartz_cleartmp(Q);
	return err;
}

quartz_Errno quartzI_runTopEntry(quartz_Thread *Q) {
	quartz_CallEntry *e = quartzI_getCallEntry(Q, 0);
	if(e == NULL) return QUARTZ_OK; // don't ask
	if(quartzI_isInterpretedFunction(e->f)) {
		return quartz_vmExec(Q);
	}
	if(e->c.k != NULL) {
		// continuation
		return e->c.k(Q, QUARTZ_OK, e->c.ku);
	}
	return quartzI_callCFunc(Q, quartzI_getCFunction(e->f));
}
