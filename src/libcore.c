#include "quartz.h"

static quartz_Errno quartz_libcore_len(quartz_Thread *Q, size_t argc) {
	quartz_Errno err;
	size_t x = quartz_len(Q, 0, &err);
	if(err) return err;
	err = quartz_pushint(Q, x);
	if(err) return err;
	quartz_return(Q, -1);
	return QUARTZ_OK;
}

static quartz_Errno quartz_libcore_cap(quartz_Thread *Q, size_t argc) {
	quartz_Errno err;
	size_t x = quartz_cap(Q, 0, &err);
	if(err) return err;
	err = quartz_pushint(Q, x);
	if(err) return err;
	quartz_return(Q, -1);
	return QUARTZ_OK;
}

static quartz_Errno quartz_libcore_type(quartz_Thread *Q, size_t argc) {
	const char *s = quartz_typenameof(Q, 0);
	quartz_Errno err = quartz_pushstring(Q, s);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libcore_load(quartz_Thread *Q, size_t argc) {
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
	err = quartz_pushscriptx(Q, 0, 1, 2);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libcore_append(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	if(argc == 0) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "no container");
	}
	err = quartz_append(Q, argc - 1);
	if(err) return err;
	return quartz_return(Q, -1);
}

quartz_Errno quartz_openlibcore(quartz_Thread *Q) {
	quartz_Errno err;

	quartz_Context *c = quartz_getContextOf(Q);
	const char *_PATH = quartz_getModulePath(c);
	const char *_CPATH = quartz_getModuleCPath(c);
	const char *_PATHCONF = quartz_getModulePathConf(c);
	
	// TODO: like, everything
	
	err = quartz_pushstring(Q, quartz_version());
	if(err) return err;
	err = quartz_setglobal(Q, "_VERSION", -1);
	if(err) return err;
	quartz_pop(Q);

	err = quartz_pushstring(Q, _PATH);
	if(err) return err;
	err = quartz_setglobal(Q, "_PATH", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushstring(Q, _CPATH);
	if(err) return err;
	err = quartz_setglobal(Q, "_CPATH", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushstring(Q, _PATHCONF);
	if(err) return err;
	err = quartz_setglobal(Q, "_PATHCONF", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushglobals(Q);
	if(err) return err;
	err = quartz_setglobal(Q, "_G", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushloaded(Q);
	if(err) return err;
	err = quartz_setglobal(Q, "_L", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushregistry(Q);
	if(err) return err;
	err = quartz_setglobal(Q, "_R", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushcfunction(Q, quartz_libcore_len);
	if(err) return err;
	err = quartz_setglobal(Q, "len", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushcfunction(Q, quartz_libcore_cap);
	if(err) return err;
	err = quartz_setglobal(Q, "cap", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushcfunction(Q, quartz_libcore_type);
	if(err) return err;
	err = quartz_setglobal(Q, "type", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushcfunction(Q, quartz_libcore_load);
	if(err) return err;
	err = quartz_setglobal(Q, "load", -1);
	if(err) return err;
	quartz_pop(Q);
	
	err = quartz_pushcfunction(Q, quartz_libcore_append);
	if(err) return err;
	err = quartz_setglobal(Q, "append", -1);
	if(err) return err;
	quartz_pop(Q);

	return QUARTZ_OK;
}
