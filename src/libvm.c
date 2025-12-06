#include "quartz.h"
#include "value.h"

static quartz_Errno quartz_libvm_disassemble(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	if(argc == 0) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "no function");
	}
	quartz_Buffer buf;
	err = quartz_bufinit(Q, &buf, 1024);
	if(err) return quartz_oom(Q);

	err = quartz_disassemble(Q, 0, &buf);
	if(err) {
		quartz_bufdestroy(&buf);
		return err;
	}

	err = quartz_pushlstring(Q, buf.buf, buf.len);
	if(err) {
		quartz_bufdestroy(&buf);
		return err;
	}

	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libvm_load(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	if(quartz_getstacksize(Q) == 1) {
		// code specified, but no name or globals
		err = quartz_pushstring(Q, "unnamed");
		if(err) return err;
	}
	if(quartz_getstacksize(Q) == 2) {
		// code and name specified, but no globals
		err = quartz_pushglobals(Q);
		if(err) return err;
	}
	if(quartz_getstacksize(Q) == 3) {
		// code, name and globals specified, but no module
		err = quartz_pushmap(Q);
		if(err) return err;
	}
	err = quartz_pushscriptx(Q, 0, 1, 2, 3);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libvm_calldepth(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	err = quartz_pushint(Q, quartz_callDepth(Q));
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libvm_stacktrace(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_Buffer buf;
	err = quartz_bufinit(Q, &buf, 1024);
	if(err) return err;
	if(argc > 0) {
		err = quartz_cast(Q, 0, QUARTZ_TSTR);
		if(err) goto cleanup;
		size_t len;
		const char *s = quartz_tolstring(Q, 0, &len, &err);
		if(err) goto cleanup;
		err = quartz_bufputf(&buf, "%z\n", len, s);
		if(err) goto cleanup;
	}
	err = quartz_bufputs(&buf, "stacktrace:\n");
	if(err) goto cleanup;
	quartz_CallInfo info;
	size_t maxDepth = 10;
	size_t depth = quartz_callDepth(Q);
	bool massive = depth > maxDepth;
	if(massive) depth = maxDepth;
	for(size_t i = 1; i < depth; i++) {
		err = quartz_requestCallInfo(Q, i, QUARTZ_CALLREQ_SOURCE, &info);
		if(err) goto cleanup;
		size_t len;
		const char *s = quartz_tolstring(Q, -1, &len, &err);
		if(err) goto cleanup;

		err = quartz_bufputf(&buf, "\t%z:%u\n", len, s, (quartz_Uint)info.curline);
		if(err) goto cleanup;
	}
	if(massive) {
		err = quartz_bufputs(&buf, "\t...\n");
		if(err) goto cleanup;
	}
	err = quartz_pushlstring(Q, buf.buf, buf.len);
	if(err) goto cleanup;
	err = quartz_return(Q, -1);
cleanup:
	quartz_bufdestroy(&buf);
	return err;
}

quartz_Errno quartz_openlibvm(quartz_Thread *Q) {
	quartz_Errno err;
	err = quartz_pushcfunction(Q, quartz_libvm_stacktrace);
	if(err) return err;
	err = quartz_seterrorhandler(Q);
	if(err) return err;
	quartz_LibFunction funcs[] = {
		{"disassemble", quartz_libvm_disassemble},
		{"load", quartz_libvm_load},
		{"calldepth", quartz_libvm_calldepth},
		{"stacktrace", quartz_libvm_stacktrace},

		{NULL, NULL},
	};
	return quartz_addgloballib(Q, "vm", funcs);
}

