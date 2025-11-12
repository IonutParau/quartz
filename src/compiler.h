#ifndef QUARTZ_COMPILER_H
#define QUARTZ_COMPILER_H

#include "quartz.h"
#include "value.h"
#include "lexer.h"
#include "parser.h"

// not gonna write the proof here, but the index in the list always matches stack index.
// However, the list is backwards.
// Because of that, the full equasion is localc - i - 1
// Same things for upvalues
typedef struct quartz_Local {
	quartz_String *name;
	struct quartz_Local *next;
} quartz_Local;

typedef struct quartz_UpvalDesc {
	quartz_Upvalue info;
	struct quartz_UpvalDesc *next;
} quartz_UpvalDesc;

// to store and restore
typedef struct quartz_Scope {
	short localc;
	short stacksize;
} quartz_Scope;

typedef struct quartz_Constants {
	// original token contents
	const char *str;
	size_t strlen;
	// next guy
	struct quartz_Constants *next;
	// value
	quartz_Value val;
} quartz_Constants;

typedef struct quartz_Compiler {
	quartz_Thread *Q;
	// for outer->scope and outer->constants
	struct quartz_Compiler *outer;
	quartz_Local *localList;
	quartz_Local *upvalList;
	quartz_Constants *constants;
	quartz_Instruction *code;
	unsigned short upvalc;
	unsigned short localc;
	unsigned short constantsCount;
	unsigned short curstacksize;
	size_t codesize;
	size_t codecap;
} quartz_Compiler;

quartz_Errno quartzC_initCompiler(quartz_Thread *Q, quartz_Compiler *c);
void quartzC_freeFailingCompiler(quartz_Compiler c);
quartz_Function *quartzC_toFunctionAndFree(quartz_Compiler *c);

quartz_Errno quartzC_writeInstruction(quartz_Compiler *c, quartz_Instruction inst);

quartz_Value quartzC_findConstant(quartz_Compiler *c, const char *str, size_t len);
quartz_Errno quartzC_addConstant(quartz_Compiler *c, const char *str, size_t len, quartz_Value val);
quartz_Errno quartzC_pushValue(quartz_Compiler *c, quartz_Node *node);
quartz_Errno quartzC_setValue(quartz_Compiler *c, quartz_Node *node, quartz_Node *to);

#endif
