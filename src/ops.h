#ifndef QUARTZ_OPS_H
#define QUARTZ_OPS_H

// TODO: make this a register-based bytecode for more perf
typedef enum quartz_OpCode {
	QUARTZ_OP_NOP = 0, // do nothing
	QUARTZ_OP_RETMOD, // return current module
	QUARTZ_OP_PUSHINT, // push uD as quartz_Int
	QUARTZ_OP_PUSHCONST, // push consts[uD]
	QUARTZ_OP_GETEXTERN, // load entry named consts[uD] from the module or globals table
	QUARTZ_OP_LOAD, // push value at index uD
	QUARTZ_OP_CALL, // invoke f with uD args, call flags are in A.
	QUARTZ_OP_GETFIELD, // pop field, pop value, push value[field]
	QUARTZ_OP_GETCONSTFIELD, // like getfield except the field is the constant in uD.
	QUARTZ_OP_VARARGPREP, // handles ..., the intended arg count is in uD.
	QUARTZ_OP_EXECOP, // B is 2 for binops and C is the BinOp.
	QUARTZ_OP_LIST, // length is uD
	QUARTZ_OP_PUSHNULL, // push null uD+1 times
	QUARTZ_OP_PUSHBOOL, // pushes boolean that checks if uD != 0, the instruction was suggested by Blendi after observing a supreme lack of QUARTZ_OP_PUSHBOOL
	QUARTZ_OP_JMP, // IP += sD
	QUARTZ_OP_DUP, // dup(uD + 1)
	QUARTZ_OP_POP, // pop(uD + 1)
	// we do not pop, because we can abuse these to save some cycles for and and or statements
	QUARTZ_OP_CJMP, // if(stack[-1]) IP += sD
	QUARTZ_OP_CNJMP, // if(!stack[-1]) IP += sD
	QUARTZ_OP_PCJMP, // if(pop()) IP += sD
	QUARTZ_OP_PCNJMP, // if(!pop()) IP += sD
	QUARTZ_OP_SETFIELD, // pop value, pop field, pop container, set container[field] = value
	QUARTZ_OP_SETCONSTFIELD, // like getfield except the field is the constant in uD.
	QUARTZ_OP_SETMODULE, // set module entry for key in consts[uD]
	QUARTZ_OP_STORE, // pop value and put it in index uD
	QUARTZ_OP_LOADUPVAL, // push ups[uD]
	QUARTZ_OP_STOREUPVAL, // pop value, set ups[uD] = value
	QUARTZ_OP_SETSTACK, // set stack size of uD
	QUARTZ_OP_ITERATE, // call quartz_iterate()
	QUARTZ_OP_ITERJMP, // if the key is null, pop the 3 values and IP += sD
	QUARTZ_OP_RET, // if A is 0, return nothing. If A is 1, return top value
	QUARTZ_OP_PUSHCLOSURE, // push consts[uD]

	QUARTZ_VMOPS_COUNT, // count
} quartz_OpCode;

typedef enum quartz_OpDisArgFlags {
	QUARTZ_DISARG_A = 1<<0,
	QUARTZ_DISARG_B = 1<<1,
	QUARTZ_DISARG_C = 1<<2,
	QUARTZ_DISARG_uD = 1<<3,
	QUARTZ_DISARG_sD = 1<<4,
	QUARTZ_DISARG_uD_const = 1<<5,
	QUARTZ_DISARG_uD_upval = 1<<6,
	QUARTZ_DISARG_sD_relrip = 1<<7,
} quartz_OpDisArgFlags;

// info for disassembly
typedef struct quartz_OpDisInfo {
	const char *name;
	quartz_OpDisArgFlags args;
} quartz_OpDisInfo;

extern quartz_OpDisInfo quartzI_disInfo[QUARTZ_VMOPS_COUNT];

#endif
