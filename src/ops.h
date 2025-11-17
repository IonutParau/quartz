#ifndef QUARTZ_OPS_H
#define QUARTZ_OPS_H

// TODO: make this a register-based bytecode for more perf
typedef enum quartz_OpCode {
	QUARTZ_OP_NOP = 0, // do nothing
	QUARTZ_OP_RETMOD, // return current module
	QUARTZ_OP_PUSHINT, // push sD as quartz_Int
	QUARTZ_OP_PUSHCONST, // push consts[uD]
	QUARTZ_OP_GETEXTERN, // load entry named consts[uD] from the module or globals table
	QUARTZ_OP_LOAD, // push value at index uD
	QUARTZ_OP_CALL, // invoke f with B args, call flags are in C.
	QUARTZ_OP_GETFIELD, // pop field, pop value, push value[field]
	QUARTZ_OP_GETCONSTFIELD, // like getfield except the field is the constant in uD.
	QUARTZ_OP_VARARGPREP, // handles ..., the intended arg count is in uD.
} quartz_OpCode;

#endif
