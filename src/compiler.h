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

typedef struct quartz_Scope {
	quartz_Local *localList;
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
	quartz_Scope scope;
	quartz_Local *upvalList;
	quartz_Constants *constants;
	quartz_Instruction *code;
	short upvalc;
	short constantsCount;
	short curstacksize;
	size_t codesize;
	size_t codecap;
} quartz_Compiler;

quartz_Errno quartzI_initCompiler(quartz_Thread *Q, quartz_Compiler *c);
void quartzI_freeFailingCompiler(quartz_Thread *Q, quartz_Compiler *c);
quartz_Function *quartzI_toFunctionAndFree(quartz_Compiler *c);

quartz_Errno quartzI_writeInstruction(quartz_Compiler *c, quartz_Instruction inst);

quartz_Errno quartzI_pushValue(quartz_Compiler *c, quartz_Node *node);
quartz_Errno quartzI_setValue(quartz_Compiler *c, quartz_Node *node, quartz_Node *to);

#endif
