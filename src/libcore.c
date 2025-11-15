#include "quartz.h"

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

	return QUARTZ_OK;
}
