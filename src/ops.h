#ifndef QUARTZ_OPS_H
#define QUARTZ_OPS_H

// the value of the first constant
#define QUARTZ_CONSTREG 128
// the value of the first upvalue
#define QUARTZ_UPREG 192

// TODO: make this a register-based bytecode for more perf
typedef enum quartz_OpCode {
	QUARTZ_OP_NOP, // do nothing
	QUARTZ_OP_DUP, // duplicate top element
	QUARTZ_OP_LOAD, // push R[B]
	QUARTZ_OP_STORE, // pop and store in R[B]
	QUARTZ_OP_PUSHI, // push sD
	QUARTZ_OP_GETINDEX, // pop index, pop t, push t[index]
	QUARTZ_OP_SETINDEX, // pop v, pop index, pop t, set t[index] = v
	QUARTZ_OP_CALL, // pop uD args, pop f, push f(...args)
	QUARTZ_OP_SCALL, // pop uD, pop f, execute f(...args), discard output
	QUARTZ_OP_TRY, // pop uD, pop f, push result of try f(...args)
	QUARTZ_OP_STRY, // pop uD, pop f, execute try f(...args), discard output
	QUARTZ_OP_RET, // pop top value, store it in return register, then exit function
	QUARTZ_OP_JMP, // IP += sD
	QUARTZ_OP_JMPTRUE, // pop value, if truthy, jump.
	QUARTZ_OP_JMPFALSE, // pop value, if falsy, jump.
	QUARTZ_OP_LIST, // pop uD values, push a list with uD values
	QUARTZ_OP_TUPLE, // pop uD values, push a tuple with uD values
	QUARTZ_OP_SET, // pop uD values, push a set with uD values
	QUARTZ_OP_MAP, // pop uD values, push a set with uD values
	QUARTZ_OP_STRUCT, // load B as a constant for the fields tuple, then pop the values, and push out the newly created struct.
	QUARTZ_OP_XLOAD, // loads a global symbol, by first loading it from the module, and if it is null, load it from the global module
	QUARTZ_OP_MSTORE, // take R[B] as the variable name (likely string) and store it in the module map
	QUARTZ_OP_GSTORE, // take R[B] as the variable name (likely string) and store iti n the global map
	QUARTZ_OP_ADD, // pop b, pop a, push a + b
} quartz_OpCode;

#endif
