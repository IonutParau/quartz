#include "quartz.h"
#include "parser.h"
#include "value.h"
#include "compiler.h"
#include "gc.h"
#include "ops.h"

quartz_Errno quartzC_initCompiler(quartz_Thread *Q, quartz_Compiler *c) {
	quartz_Errno err;
	c->Q = Q;
	c->outer = NULL;
	c->localList = NULL;
	c->upvalList = NULL;
	c->constants = NULL;
	c->upvalc = 0;
	c->localc = 0;
	c->constantsCount = 0;
	c->curstacksize = 0;
	c->curstacksize = 0;
	c->argc = 0;
	c->funcflags = 0;
	c->codesize = 0;

	c->codecap = 256;
	c->code = quartz_alloc(Q, sizeof(quartz_Instruction) * c->codecap);
	return QUARTZ_OK;
}

static void quartzC_freeCompilerNoCode(quartz_Compiler c) {
	quartz_Thread *Q = c.Q;
	quartz_Constants *curConst = c.constants;
	while(curConst) {
		quartz_Constants *cur = curConst;
		curConst = curConst->next;
		quartz_free(Q, cur, sizeof(*cur));
	}
}

void quartzC_freeFailingCompiler(quartz_Compiler c) {
	quartz_free(c.Q, c.code, c.codecap * sizeof(quartz_Instruction));
	quartzC_freeCompilerNoCode(c);
}

size_t quartzC_countConstants(quartz_Compiler *c) {
	while(c->outer) c = c->outer;
	return c->constantsCount;
}

static quartz_Constants *quartzC_getConstants(quartz_Compiler *c) {
	while(c->outer) c = c->outer;
	return c->constants;
}

quartz_Function *quartzC_toFunctionAndFree(quartz_Compiler *c, quartz_String *source, quartz_Map *globals) {
	// TODO: handle OOM better
	quartz_Thread *Q = c->Q;

	quartz_Map *module = quartzI_newMap(Q, 0);
	quartz_Function *f = (quartz_Function *)quartzI_allocObject(Q, QUARTZ_OFUNCTION, sizeof(quartz_Function));
	f->module = module;
	f->globals = globals;
	// TODO: preallocate
	f->stacksize = 0;
	f->argc = c->argc;
	f->flags = c->funcflags;
	f->code = c->code;
	for(size_t i = c->codesize; i < c->codecap; i++) {
		f->code[i].op = QUARTZ_OP_NOP;
		f->code[i].line = 0;
	}
	f->codesize = c->codecap;
	f->chunkname = source;
	f->constCount = quartzC_countConstants(c);
	f->upvalCount = c->upvalc;
	// gosh darn constants
	quartz_Upvalue *upvaldefs = quartz_alloc(Q, sizeof(quartz_Upvalue) * c->upvalc);
	quartz_UpvalDesc *curUp = c->upvalList;
	for(size_t i = 0; i < c->upvalc; i++, curUp = curUp->next) {
		upvaldefs[c->upvalc - i - 1] = curUp->info;
	}
	f->upvaldefs = upvaldefs;

	quartz_Constants *curConst = quartzC_getConstants(c);
	quartz_Value *consts = quartz_alloc(Q, sizeof(quartz_Value) * f->constCount);
	for(size_t i = 0; i < f->constCount; i++, curConst = curConst->next) {
		consts[f->constCount - i - 1] = curConst->val;
	}
	f->consts = consts;
	quartzC_freeCompilerNoCode(*c);
	
	return f;
}

quartz_Errno quartzC_writeInstruction(quartz_Compiler *c, quartz_Instruction inst) {
	quartz_Thread *Q = c->Q;
	if(c->codesize == c->codecap) {
		size_t newCap = c->codecap * 2;
		quartz_Instruction *newCode = quartz_realloc(Q, c->code, sizeof(quartz_Instruction) * c->codecap, sizeof(quartz_Instruction) * newCap);
		if(newCode == NULL) return quartz_oom(Q);
		c->code = newCode;
		c->codecap = newCap;
	}

	c->code[c->codesize++] = inst;
	return QUARTZ_OK;
}

quartz_Value quartzC_findConstant(quartz_Compiler *c, const char *str, size_t len);

quartz_Errno quartzC_addConstant(quartz_Compiler *c, const char *str, size_t len, quartz_Value val) {
	quartz_Thread *Q = c->Q;
	while(c->outer) c = c->outer;
	quartz_Constants *newNode = quartz_alloc(Q, sizeof(quartz_Constants));
	if(newNode == NULL) return quartz_oom(Q);
	newNode->next = c->constants;
	newNode->str = str;
	newNode->strlen = len;
	newNode->val = val;
	c->constants = newNode;
	c->constantsCount++;
	return QUARTZ_OK;
}
