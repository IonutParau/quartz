#include "ops.h"
#include "quartz.h"
#include "value.h"
#include "state.h"
#include "gc.h"

quartz_Errno quartzI_addCallEntry(quartz_Thread *Q, quartz_CallEntry *entry) {
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
		.errorh = quartzI_currentErrorHandler(Q),
	};
	if(quartzI_isInterpretedFunction(f)) {
		quartz_Function *q = quartzI_getFunction(f);
		e.q.pc = q->code;
	} else {
		e.c.k = NULL;
		e.c.ku = NULL;
	}
	err = quartzI_addCallEntry(Q, &e);
	if(err) {
		return err;
	}
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
			// safety check
			quartz_Uint ss = quartz_getstacksize(Q);
			// argv
			err = quartz_assertf(Q, ss == 0, QUARTZ_ERUNTIME, "bad stack size (%u)", ss);
			if(err) return err;
			Q->stack[c->funcStackIdx] = (quartz_StackEntry) {
				.isPtr = false,
				.value.type = QUARTZ_VOBJ,
				.value.obj = &f->module->obj,
			};
			goto done;
		} else if(pc->op == QUARTZ_OP_PUSHINT) {
			err = quartz_pushint(Q, pc->uD);
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
			err = quartz_call(Q, pc->uD, pc->A);
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
		} else if(pc->op == QUARTZ_OP_EXECOP) {
			if(pc->B == 2) {
				err = quartz_binop(Q, pc->C);
				if(err) goto done;
			} else {
				err = quartz_errorf(Q, QUARTZ_ERUNTIME, "bad opcode: %u", (quartz_Uint)pc->op);
				goto done;
			}
		} else if(pc->op == QUARTZ_OP_LIST) {
			err = quartz_pushlist(Q, pc->uD);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_PUSHNULL) {
			err = quartz_setstacksize(Q, quartz_getstacksize(Q) + pc->uD + 1);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_PUSHBOOL) {
			err = quartz_pushbool(Q, pc->uD != 0);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_JMP) {
			pc += pc->sD;
			continue;
		} else if(pc->op == QUARTZ_OP_DUP) {
			err = quartz_dupn(Q, pc->uD + 1);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_POP) {
			err = quartz_popn(Q, pc->uD + 1); // whats popn gng ??????
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_CJMP) {
			if(quartz_truthy(Q, -1)) {
				pc += pc->sD;
				continue;
			}
		} else if(pc->op == QUARTZ_OP_CNJMP) {
			if(!quartz_truthy(Q, -1)) {
				pc += pc->sD;
				continue;
			}
		} else if(pc->op == QUARTZ_OP_PCJMP) {
			bool b = quartz_truthy(Q, -1);
			err = quartz_pop(Q);
			if(err) return err;
			if(b) {
				pc += pc->sD;
				continue;
			}
		} else if(pc->op == QUARTZ_OP_PCNJMP) {
			bool b = quartz_truthy(Q, -1);
			err = quartz_pop(Q);
			if(err) return err;
			if(!b) {
				pc += pc->sD;
				continue;
			}
		} else if(pc->op == QUARTZ_OP_SETFIELD) {
			err = quartz_setindex(Q);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_SETCONSTFIELD) {
			quartz_Value k = f->consts[pc->uD];
			quartz_Value container = quartzI_getStackValue(Q, -2);
			quartz_Value v = quartzI_getStackValue(Q, -1);
			err = quartzI_setIndex(Q, container, k, v);
			if(err) goto done;
			err = quartz_popn(Q, 2);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_SETMODULE) {
			quartz_Value k = f->consts[pc->uD];
			quartz_Value v = quartzI_getStackValue(Q, -1);
			err = quartzI_mapSet(Q, f->module, k, v);
			if(err) goto done;
			err = quartz_pop(Q);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_STORE) {
			err = quartz_copy(Q, -1, pc->uD);
			if(err) goto done;
			err = quartz_pop(Q);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_LOADUPVAL) {
			err = quartzI_pushRawValue(Q, closure->ups[pc->uD]->val);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_STOREUPVAL) {
			quartz_Value val = quartzI_getStackValue(Q, -1);
			if(err) goto done;
			closure->ups[pc->uD]->val = val;
			err = quartz_pop(Q);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_SETSTACK) {
			err = quartz_setstacksize(Q, pc->uD);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_ITERATE) {
			err = quartz_iterate(Q);
			if(err) goto done;
		} else if(pc->op == QUARTZ_OP_ITERJMP) {
			if(quartz_typeof(Q, -2) == QUARTZ_TNULL) {
				// iter over, do jmp
				pc += pc->sD;
				continue;
			}
			// iter not over, do nothing
		} else if(pc->op == QUARTZ_OP_RET) {
			if(pc->A == 0) {
				goto done;
			}
			err = quartz_return(Q, -1);
			goto done;
		} else if(pc->op == QUARTZ_OP_PUSHCLOSURE) {
			quartz_Value fv = f->consts[pc->uD];
			quartz_Function *cf = quartzI_getFunction(fv);
			quartz_Closure *clos = quartzI_newClosure(Q, fv, cf->upvalCount);
			if(clos == NULL) {
				err = quartz_oom(Q);
				goto done;
			}
			for(size_t i = 0; i < cf->upvalCount; i++) {
				quartz_Upvalue up = cf->upvaldefs[i];
				if(up.inStack) {
					quartz_Pointer *p = quartzI_getStackValuePointer(Q, up.stackIndex, &err);
					if(err) goto done;
					clos->ups[i] = p;
				} else {
					clos->ups[i] = closure->ups[i];
				}
			}
			err = quartzI_pushRawValue(Q, (quartz_Value) {
				.type = QUARTZ_VOBJ,
				.obj = &clos->obj,
			});
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

size_t quartz_callDepth(quartz_Thread *Q) {
	return Q->callLen;
}

quartz_Errno quartz_requestCallInfo(quartz_Thread *Q, size_t off, quartz_CallRequest request, quartz_CallInfo *info) {
	quartz_CallEntry *e = quartzI_getCallEntry(Q, off);
	if(e == NULL) {
		info->valid = false;
		return QUARTZ_OK;
	}
	quartz_Errno err = QUARTZ_OK;
	info->valid = true;

	if(request & QUARTZ_CALLREQ_SOURCE) {
		quartz_Function *f = quartzI_getFunction(e->f);
		if(f == NULL) {
			info->what = "C";
			info->curline = 0;
			err = quartz_pushstring(Q, "[C]");
			if(err) return err;
		} else {
			info->what = "Quartz";
			info->curline = e->q.pc->line;
			err = quartzI_pushRawValue(Q, (quartz_Value) {
				.type = QUARTZ_VOBJ,
				.obj = &f->chunkname->obj,
			});
			if(err) return err;
		}
	}
	
	if(request & QUARTZ_CALLREQ_FUNC) {
		info->flags = e->flags;
		err = quartzI_pushRawValue(Q, e->f);
		if(err) return err;
		quartz_Function *f = quartzI_getFunction(e->f);
		if(f == NULL) {
			info->isVararg = true;
			info->argc = 0;
		} else {
			info->isVararg = f->flags & QUARTZ_FUNC_VARARGS;
			info->argc = f->argc;
		}
	}

	return QUARTZ_OK;
}
