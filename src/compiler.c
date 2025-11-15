#include "quartz.h"
#include "parser.h"
#include "utils.h"
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
	c.constants = NULL;
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

	size_t constCount = quartzC_countConstants(c);
	
	quartz_Upvalue *upvaldefs = quartz_alloc(Q, sizeof(quartz_Upvalue) * c->upvalc);
	quartz_UpvalDesc *curUp = c->upvalList;
	for(size_t i = 0; i < c->upvalc; i++, curUp = curUp->next) {
		upvaldefs[c->upvalc - i - 1] = curUp->info;
	}
	
	quartz_Constants *curConst = quartzC_getConstants(c);
	quartz_Value *consts = quartz_alloc(Q, sizeof(quartz_Value) * constCount);
	for(size_t i = 0; i < constCount; i++, curConst = curConst->next) {
		consts[constCount - i - 1] = curConst->val;
	}

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
	f->constCount = constCount;
	f->upvalCount = c->upvalc;
	// gosh darn constants
	f->upvaldefs = upvaldefs;

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

quartz_Int quartzC_findConstant(quartz_Compiler *c, const char *str, size_t len) {
	while(c->outer) c = c->outer;
	quartz_Constants *list = c->constants;
	size_t i = 0;
	while(list) {
		if(quartzI_strleql(list->str, list->strlen, str, len)) {
			return c->constantsCount - i - 1;
		}
		list = list->next;
		i++;
	}
	return -1;
}

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

static quartz_Errno quartzC_pushChildArray(quartz_Compiler *c, quartz_Node *node) {
	quartz_Errno err = QUARTZ_OK;
	for(size_t i = 0; i < node->childCount; i++) {
		err = quartzC_pushValue(c, node->children[i]);
		if(err) return err;
	}
	return err;
}

quartz_Errno quartzC_internString(quartz_Compiler *c, const char *str, size_t len, size_t *idx) {
	quartz_Int found = quartzC_findConstant(c, str, len);
	if(found < 0) {
		// shit we gotta make it ourselves
		quartz_Errno err;
		*idx = quartzC_countConstants(c);
		size_t trueSize = quartzI_trueStringLen(str, len);
		quartz_String *s = quartzI_newString(c->Q, trueSize, NULL);
		if(s == NULL) return quartz_oom(c->Q);
		quartzI_trueStringWrite(s->buf, str, len);
		err = quartzC_addConstant(c, str, len, (quartz_Value) {.type = QUARTZ_VOBJ, .obj = &s->obj});
		if(err) return err;
		return QUARTZ_OK;
	}
	*idx = found;
	return QUARTZ_OK;
}

quartz_Errno quartzC_pushValue(quartz_Compiler *c, quartz_Node *node) {
	quartz_Thread *Q = c->Q;
	quartz_Errno err;

	if(node->type == QUARTZ_NODE_VARIABLE) {
		// this one's a little complicated

		// TODO: check if local
		// TODO: check if upvalue 

		// must be a global
		size_t globalConst;
		err = quartzC_internString(c, node->str, node->strlen, &globalConst);
		if(err) return err;
		return quartzC_writeInstruction(c, (quartz_Instruction) {
			.op = QUARTZ_OP_GETEXTERN,
			.uD = globalConst,
			.line = node->line,
		});
	}

	if(node->type == QUARTZ_NODE_STR) {
		size_t constID;
		err = quartzC_internString(c, node->str, node->strlen, &constID);
		if(err) return err;
		return quartzC_writeInstruction(c, (quartz_Instruction) {
			.op = QUARTZ_OP_PUSHCONST,
			.uD = constID,
			.line = node->line,
		});
	}

	if(node->type == QUARTZ_NODE_FIELD) {
		err = quartzC_pushValue(c, node->children[0]);
		if(err) return err;
		size_t constID;
		err = quartzC_internString(c, node->str, node->strlen, &constID);
		if(err) return err;
		return quartzC_writeInstruction(c, (quartz_Instruction) {
			.op = QUARTZ_OP_GETCONSTFIELD,
			.uD = constID,
			.line = node->line,
		});
	}

	if(node->type == QUARTZ_NODE_NUM) {
		// this DOES NOT WORK FOR FLOATS but we DO NOT CARE RIGHT NOW
		// TODO: make it actually check what type of number it is
		quartz_Int i = quartzI_atoi(node->str, node->strlen);
		return quartzC_writeInstruction(c, (quartz_Instruction) {
			.op = QUARTZ_OP_PUSHINT,
			.sD = i,
		});
	}
	
	if(node->type == QUARTZ_NODE_CALL) {
		err = quartzC_pushChildArray(c, node);
		if(err) return err;
		size_t argc = node->childCount - 1;
		err = quartzC_writeInstruction(c, (quartz_Instruction) {
			.op = QUARTZ_OP_CALL,
			.B = argc,
			.C = 0,
			.line = node->line,
		});
		return QUARTZ_OK;
	}

	return quartz_errorf(Q, QUARTZ_ERUNTIME, "bad expression node: %u (line %u)", (quartz_Uint)node->type, (quartz_Uint)node->line);
}

quartz_Errno quartzC_setValue(quartz_Compiler *c, quartz_Node *node, quartz_Node *to);

quartz_Errno quartzC_runStatement(quartz_Compiler *c, quartz_Node *node) {
	quartz_Thread *Q = c->Q;
	quartz_Errno err;
	if(node->type == QUARTZ_NODE_CALL) {
		err = quartzC_pushChildArray(c, node);
		if(err) return err;
		size_t argc = node->childCount - 1;
		err = quartzC_writeInstruction(c, (quartz_Instruction) {
			.op = QUARTZ_OP_CALL,
			.B = argc,
			.C = QUARTZ_CALL_STATIC,
			.line = node->line,
		});
		return QUARTZ_OK;
	}

	return quartz_errorf(Q, QUARTZ_ERUNTIME, "bad statement node: %u (line %u)", (quartz_Uint)node->type, (quartz_Uint)node->line);
}

quartz_Errno quartzC_compileProgram(quartz_Compiler *c, quartz_Node *tree) {
	quartz_Errno err;
	
	// TODO: prepend varargprep instruction
	
	for(size_t i = 0; i < tree->childCount; i++) {
		err = quartzC_runStatement(c, tree->children[i]);
		if(err) return err;
	}

	err = quartzC_writeInstruction(c, (quartz_Instruction) {
		.op = QUARTZ_OP_RETMOD,
		.line = tree->line,
	});
	if(err) return err;
	return QUARTZ_OK;
}
