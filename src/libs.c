#include "quartz.h"

quartz_Errno quartz_openstdlib(quartz_Thread *Q) {
	quartz_Errno err;
	err = quartz_openlibcore(Q);
	if(err) return err;
	err = quartz_openlibio(Q);
	if(err) return err;
	err = quartz_openlibfs(Q);
	if(err) return err;
	err = quartz_openlibgc(Q);
	if(err) return err;
	err = quartz_openlibvm(Q);
	if(err) return err;
	err = quartz_openlibbuf(Q);
	if(err) return err;
	return QUARTZ_OK;
}

quartz_Errno quartz_addlib(quartz_Thread *Q, const char *module, quartz_LibFunction *libs) {
	quartz_Errno err;

	size_t oldstack = quartz_getstacksize(Q);

	// thing is top of stack
	err = quartz_pushmap(Q);
	if(err) goto error;
	err = quartz_setloaded(Q, module, -1);
	if(err) goto error;

	for(size_t i = 0; libs[i].name != NULL; i++) {
		err = quartz_dup(Q);
		if(err) goto error;
		err = quartz_pushstring(Q, libs[i].name);
		if(err) goto error;
		err = quartz_pushcfunction(Q, libs[i].f);
		if(err) goto error;
		err = quartz_setindex(Q);
		if(err) goto error;
	}

	// pop the thing
	err = quartz_pop(Q);
	if(err) goto error;

	return QUARTZ_OK;
error:
	quartz_setstacksize(Q, oldstack);
	return err;
}

quartz_Errno quartz_addgloballib(quartz_Thread *Q, const char *global, quartz_LibFunction *libs) {
	quartz_Errno err;

	size_t oldstack = quartz_getstacksize(Q);

	// thing is top of stack
	err = quartz_pushmap(Q);
	if(err) goto error;
	err = quartz_setloaded(Q, global, -1);
	if(err) goto error;
	err = quartz_setglobal(Q, global, -1);
	if(err) goto error;

	for(size_t i = 0; libs[i].name != NULL; i++) {
		err = quartz_dup(Q);
		if(err) goto error;
		err = quartz_pushstring(Q, libs[i].name);
		if(err) goto error;
		err = quartz_pushcfunction(Q, libs[i].f);
		if(err) goto error;
		err = quartz_setindex(Q);
		if(err) goto error;
	}

	// pop the thing
	err = quartz_pop(Q);
	if(err) goto error;

	return QUARTZ_OK;
error:
	quartz_setstacksize(Q, oldstack);
	return err;
}

quartz_Errno quartz_searchPath(quartz_Thread *Q, const char *module);
quartz_Errno quartz_import(quartz_Thread *Q, const char *module);
