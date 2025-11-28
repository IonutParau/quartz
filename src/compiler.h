#ifndef QUARTZ_COMPILER_H
#define QUARTZ_COMPILER_H

#include "quartz.h"
#include "value.h"
#include "lexer.h"
#include "parser.h"

typedef struct quartz_Local {
	const char *str;
	unsigned int strlen;
	unsigned int index;
	struct quartz_Local *next;
} quartz_Local;

typedef struct quartz_UpvalDesc {
	const char *str;
	unsigned int strlen;
	unsigned int index;
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
	quartz_String *source;
	quartz_Map *globals;
	quartz_Map *module;
	quartz_Local *localList;
	quartz_UpvalDesc *upvalList;
	quartz_Constants *constants;
	quartz_Instruction *code;
	unsigned short upvalc;
	unsigned short localc;
	unsigned short constantsCount;
	unsigned short curstacksize;
	unsigned short argc;
	unsigned short funcflags;
	size_t codesize;
	size_t codecap;
} quartz_Compiler;

typedef enum quartz_VarType {
	QUARTZC_VAR_LOCAL,
	QUARTZC_VAR_UPVAL,
	QUARTZC_VAR_GLOBAL,
} quartz_VarType;

typedef struct quartz_Variable {
	int type;
	int index;
} quartz_Variable;

quartz_Errno quartzC_initCompiler(quartz_Thread *Q, quartz_Compiler *c, quartz_String *source, quartz_Map *globals, quartz_Map *module);
void quartzC_freeFailingCompiler(quartz_Compiler c);
quartz_Function *quartzC_toFunctionAndFree(quartz_Compiler *c);
size_t quartzC_countConstants(quartz_Compiler *c);

quartz_Errno quartzC_writeInstruction(quartz_Compiler *c, quartz_Instruction inst);

// -1 if not found
quartz_Int quartzC_findConstant(quartz_Compiler *c, const char *str, size_t len);
quartz_Errno quartzC_addConstant(quartz_Compiler *c, const char *str, size_t len, quartz_Value val);
quartz_Errno quartzC_internString(quartz_Compiler *c, const char *str, size_t len, size_t *idx);

// effectively resolves a name, and also signals the transfer of upvalues.
quartz_Errno quartzC_useVariable(quartz_Compiler *c, const char *str, size_t len, quartz_Variable *var);
quartz_Errno quartzC_defineLocal(quartz_Compiler *c, const char *name, size_t namelen);
void quartzC_popLocal(quartz_Compiler *c);
void quartzC_setLocalCount(quartz_Compiler *c, size_t localc);

quartz_Errno quartzC_pushValue(quartz_Compiler *c, quartz_Node *node);
quartz_Errno quartzC_setValue(quartz_Compiler *c, quartz_Node *node, quartz_Node *to);
quartz_Errno quartzC_runStatement(quartz_Compiler *c, quartz_Node *node);
quartz_Errno quartzC_compileProgram(quartz_Compiler *c, quartz_Node *tree);

#endif
