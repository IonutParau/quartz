#ifndef QUARTZ_OPS_H
#define QUARTZ_OPS_H

// TODO: make this a register-based bytecode for more perf
typedef enum quartz_OpCode {
	QUARTZ_OP_NOP = 0, // do nothing
	QUARTZ_OP_PUSHINT, // push sD as quartz_Int
	QUARTZ_OP_PUSHCONST, // push consts[uD]
	QUARTZ_OP_PUSHVAL, // push value at stack index uD
	QUARTZ_OP_POPVAL, // pop uD+1 values
	QUARTZ_OP_GETINDEX, // pop k, pop t, push t[k]
	QUARTZ_OP_BEGINCALL, // set top value as the function to be called (remember index)
	QUARTZ_OP_CALL, // call selected function with flags in uD.
} quartz_OpCode;

#endif
