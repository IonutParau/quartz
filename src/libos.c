#include "quartz.h"

quartz_Errno quartz_libos_spawn(quartz_Thread *Q, size_t argc) {
	quartz_Errno err;

	err = quartz_typeassert(Q, 0, QUARTZ_TSTR);
	if(err) return err;
	err = quartz_typeassert(Q, 1, QUARTZ_TLIST);
	if(err) return err;

	const char *path = quartz_tostring(Q, 0, &err);
	if(err) return err;
	
	size_t pargc = quartz_len(Q, 1, &err);
	if(err) return err;

	const char **pargv = quartz_alloc(Q, sizeof(pargv[0]) * (pargc + 1));
	if(pargv == NULL) return quartz_oom(Q);
	pargv[pargc] = NULL;

	for(size_t i = 0; i < pargc; i++) {
		err = quartz_pushvalue(Q, 1);
		if(err) goto cleanup;
		err = quartz_pushint(Q, i);
		if(err) goto cleanup;
		err = quartz_getindex(Q);
		if(err) goto cleanup;
		pargv[i] = quartz_tostring(Q, -1, &err);
		if(err) goto cleanup;
		err = quartz_pop(Q);
		if(err) goto cleanup;
	}

	quartz_OsArgs arg;
	arg.spawn.path = path;
	arg.spawn.argv = pargv;
	arg.spawn.env = NULL;
	quartz_OsRet ret;

	err = quartz_osfunc(Q, QUARTZ_OSF_SPAWN, &arg, &ret);
	if(err) goto cleanup;

	if(ret.spawnOrExec.exited) {
		err = quartz_pushint(Q, ret.spawnOrExec.exitcode);
		if(err) goto cleanup;
	} else {
		err = quartz_pushint(Q, 128 + ret.spawnOrExec.signal);
		if(err) goto cleanup;
	}
	err = quartz_return(Q, -1);
cleanup:
	if(pargv != NULL) quartz_free(Q, pargv, sizeof(pargv[0]) * (pargc + 1));
	return err;
}

quartz_Errno quartz_libos_execute(quartz_Thread *Q, size_t argc) {
	quartz_Errno err;

	const char *cmd = quartz_tostring(Q, 0, &err);
	if(err) return err;
	
	quartz_OsArgs arg;
	arg.exec.cmd = cmd;
	quartz_OsRet ret;

	err = quartz_osfunc(Q, QUARTZ_OSF_EXEC, &arg, &ret);
	if(err) return err;
	
	err = quartz_pushint(Q, ret.spawnOrExec.exitcode);
	if(err) return err;

	return quartz_return(Q, -1);
}
	
quartz_Errno quartz_openlibos(quartz_Thread *Q) {
	quartz_LibFunction libs[] = {
		{"spawn", quartz_libos_spawn},
		{"exec", quartz_libos_execute},
		{NULL, NULL},
	};
	return quartz_addgloballib(Q, "os", libs);
}
