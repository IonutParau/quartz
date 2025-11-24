#include "quartz.h"
#include "state.h"
#include "value.h"
#include "ops.h"

quartz_OpDisInfo quartzI_disInfo[QUARTZ_VMOPS_COUNT] = {
	[QUARTZ_OP_NOP] = {
		.name = "nop",
		.args = 0,
	},
	[QUARTZ_OP_RETMOD] = {
		.name = "retmod",
		.args = 0,
	},
	[QUARTZ_OP_PUSHINT] = {
		.name = "pushint",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_PUSHCONST] = {
		.name = "pushconst",
		.args = QUARTZ_DISARG_uD_const,
	},
	[QUARTZ_OP_GETEXTERN] = {
		.name = "getextern",
		.args = QUARTZ_DISARG_uD_const,
	},
	[QUARTZ_OP_LOAD] = {
		.name = "load",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_CALL] = {
		.name = "call",
		.args = QUARTZ_DISARG_A | QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_GETFIELD] = {
		.name = "getfield",
		.args = 0,
	},
	[QUARTZ_OP_GETCONSTFIELD] = {
		.name = "getconstfield",
		.args = QUARTZ_DISARG_uD_const,
	},
	[QUARTZ_OP_VARARGPREP] = {
		.name = "varargprep",
		.args = QUARTZ_DISARG_uD,
	},
	// TODO: special-case this instruction because its tricky
	[QUARTZ_OP_EXECOP] = {
		.name = "execop",
		.args = QUARTZ_DISARG_B | QUARTZ_DISARG_C,
	},
	[QUARTZ_OP_LIST] = {
		.name = "list",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_PUSHNULL] = {
		.name = "pushnull",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_PUSHBOOL] = {
		.name = "pushbool",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_JMP] = {
		.name = "jmp",
		.args = QUARTZ_DISARG_sD_relrip,
	},
	[QUARTZ_OP_DUP] = {
		.name = "dup",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_POP] = {
		.name = "pop",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_CJMP] = {
		.name = "cjmp",
		.args = QUARTZ_DISARG_sD_relrip,
	},
	[QUARTZ_OP_CNJMP] = {
		.name = "cnjmp",
		.args = QUARTZ_DISARG_sD_relrip,
	},
	[QUARTZ_OP_PCJMP] = {
		.name = "pcjmp",
		.args = QUARTZ_DISARG_sD_relrip,
	},
	[QUARTZ_OP_PCNJMP] = {
		.name = "pcnjmp",
		.args = QUARTZ_DISARG_sD_relrip,
	},
	[QUARTZ_OP_SETFIELD] = {
		.name = "setfield",
		.args = 0,
	},
	[QUARTZ_OP_SETCONSTFIELD] = {
		.name = "setconstfield",
		.args = QUARTZ_DISARG_uD_const,
	},
	[QUARTZ_OP_SETMODULE] = {
		.name = "setmodule",
		.args = QUARTZ_DISARG_uD_const,
	},
	[QUARTZ_OP_STORE] = {
		.name = "store",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_LOADUPVAL] = {
		.name = "loadupval",
		.args = QUARTZ_DISARG_uD_upval,
	},
	[QUARTZ_OP_STOREUPVAL] = {
		.name = "storeupval",
		.args = QUARTZ_DISARG_uD_upval,
	},
	[QUARTZ_OP_SETSTACK] = {
		.name = "setstack",
		.args = QUARTZ_DISARG_uD,
	},
	[QUARTZ_OP_ITERATE] = {
		.name = "iterate",
		.args = 0,
	},
	[QUARTZ_OP_ITERJMP] = {
		.name = "iterjmp",
		.args = QUARTZ_DISARG_sD_relrip,
	},
};

static quartz_Errno quartzDis_disassembleFunc(quartz_Thread *Q, quartz_Buffer *buf, quartz_Function *f) {
	int curline = -1;
	quartz_Errno err;

	err = quartz_bufputf(buf, "%p(%u args, %u constants, %u upvalues, %u code):\n", f, (quartz_Uint)f->argc, (quartz_Uint)f->constCount, (quartz_Uint)f->upvalCount, (quartz_Uint)f->codesize);
	if(err) return quartz_erroras(Q, err);

	if(f->flags & QUARTZ_FUNC_VARARGS) {
		err = quartz_bufputs(buf, ".vararg\n");
		if(err) return quartz_erroras(Q, err);
	}
	if(f->flags & QUARTZ_FUNC_CHUNK) {
		err = quartz_bufputs(buf, ".chunk\n");
		if(err) return quartz_erroras(Q, err);
	}

	quartz_String *constStrs[f->constCount];

	for(size_t i = 0; i < f->constCount; i++) {
		quartz_Value v = f->consts[i];
		quartz_String *s = quartzI_valueQuoted(Q, v);
		if(s == NULL) return quartz_oom(Q);
		constStrs[i] = s;

		err = quartz_bufputf(buf, ".const %u %s %z\n", (quartz_Uint)i, quartz_typenames[quartzI_trueTypeOf(v)], s->len, s->buf);
		if(err) return quartz_erroras(Q, err);
	}
	
	for(size_t i = 0; i < f->upvalCount; i++) {
		quartz_Upvalue u = f->upvaldefs[i];
		err = quartz_bufputf(buf, ".upval %u %s\n", (quartz_Uint)u.stackIndex, u.inStack ? "in-stack" : "stolen");
		if(err) return quartz_erroras(Q, err);
	}

	for(size_t i = 0; i < f->codesize; i++) {
		quartz_Instruction inst = f->code[i];

		if(inst.line != curline) {
			curline = inst.line;
			err = quartz_bufputf(buf, ".line %d\n", (int)curline);
			if(err) return quartz_erroras(Q, err);
		}

		err = quartz_bufputf(buf, "0x%04x: %02x%02x%02x%02x ", i, (quartz_Int)inst.op, (quartz_Int)inst.A, (quartz_Int)inst.B, (quartz_Int)inst.C);
		if(err) return quartz_erroras(Q, err);

		quartz_OpDisInfo info = quartzI_disInfo[inst.op];

		if(info.name != NULL) {
			err = quartz_bufputs(buf, info.name);
			if(err) return quartz_erroras(Q, err);

			bool firstArg = true;
			if(info.args & QUARTZ_DISARG_A) {
				err = quartz_bufputs(buf, firstArg ? " " : ", ");
				if(err) return quartz_erroras(Q, err);
				firstArg = false;

				err = quartz_bufputu(buf, inst.A);
				if(err) return quartz_erroras(Q, err);
			}
			if(info.args & QUARTZ_DISARG_B) {
				err = quartz_bufputs(buf, firstArg ? " " : ", ");
				if(err) return quartz_erroras(Q, err);
				firstArg = false;

				err = quartz_bufputu(buf, inst.B);
				if(err) return quartz_erroras(Q, err);
			}
			if(info.args & QUARTZ_DISARG_C) {
				err = quartz_bufputs(buf, firstArg ? " " : ", ");
				if(err) return quartz_erroras(Q, err);
				firstArg = false;

				err = quartz_bufputu(buf, inst.C);
				if(err) return quartz_erroras(Q, err);
			}
			
			if(info.args & QUARTZ_DISARG_sD) {
				err = quartz_bufputs(buf, firstArg ? " " : ", ");
				if(err) return quartz_erroras(Q, err);
				firstArg = false;

				err = quartz_bufputn(buf, inst.sD);
				if(err) return quartz_erroras(Q, err);
			}
			
			if(info.args & QUARTZ_DISARG_uD) {
				err = quartz_bufputs(buf, firstArg ? " " : ", ");
				if(err) return quartz_erroras(Q, err);
				firstArg = false;

				err = quartz_bufputu(buf, inst.uD);
				if(err) return quartz_erroras(Q, err);
			}
			
			if(info.args & QUARTZ_DISARG_uD_const) {
				err = quartz_bufputs(buf, firstArg ? " " : ", ");
				if(err) return quartz_erroras(Q, err);
				firstArg = false;

				err = quartz_bufputf(buf, "%u (%s)", (quartz_Uint)inst.uD, constStrs[inst.uD]->buf);
				if(err) return quartz_erroras(Q, err);
			}
			
			if(info.args & QUARTZ_DISARG_sD_relrip) {
				err = quartz_bufputs(buf, firstArg ? " " : ", ");
				if(err) return quartz_erroras(Q, err);
				firstArg = false;

				err = quartz_bufputf(buf, "%d (0x%04x)", (quartz_Int)inst.sD, (quartz_Int)(i + inst.sD));
				if(err) return quartz_erroras(Q, err);
			}
		}

		err = quartz_bufputc(buf, '\n');
		if(err) return quartz_erroras(Q, err);

		if(inst.op == QUARTZ_OP_RETMOD) {
			if(f->codesize - i > 0) {
				err = quartz_bufputf(buf, ".di %u\n", f->codesize - i - 1);
				if(err) return quartz_erroras(Q, err);
			}
			break;
		}
	}

	return QUARTZ_OK;
}

quartz_Errno quartz_disassemble(quartz_Thread *Q, int x, quartz_Buffer *buf) {
	quartz_Errno err = quartz_typeassert(Q, x, QUARTZ_TFUNCTION);
	if(err) return err;
	quartz_Value v = quartzI_getStackValue(Q, x);
	quartz_Function *f = quartzI_getFunction(v);

	if(f == NULL) {
		err = quartz_bufputf(buf, "%cfunc %p\n", quartzI_getCFunction(v));
		if(err) return quartz_erroras(Q, err);
		return QUARTZ_OK;
	}

	err = quartz_bufputf(buf, ".source \"%s\"\n", f->chunkname->buf);
	if(err) return quartz_erroras(Q, err);

	return quartzDis_disassembleFunc(Q, buf, f);
}
