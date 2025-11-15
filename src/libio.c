#include "quartz.h"

static quartz_Errno testPrint(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_File *_stdout = quartz_fstdfile(Q, QUARTZ_STDOUT);
	if(_stdout == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stdout does not exist");
	}
	size_t written = 0;
	for(size_t i = 0; i < argc; i++) {
		err = quartz_cast(Q, i, QUARTZ_TSTR);
		if(err) return err;
		size_t len;
		const char *s = quartz_tolstring(Q, i, &len, &err);
		if(err) return err;
		err = quartz_fwrite(Q, _stdout, s, &len);
		if(err) return err;
		written += len;
	}
	size_t len = 1;
	err = quartz_fwrite(Q, _stdout, "\n", &len);
	if(!err) {
		written += len;
		err = quartz_pushint(Q, written);
		if(err) return err;
		err = quartz_return(Q, -1);
	}
	return err;
}

quartz_Errno quartz_openlibio(quartz_Thread *Q) {
	quartz_Errno err = quartz_fopenstdio(Q);
	if(err) return err;
	quartz_LibFunction libio[] = {
		{"writeln", testPrint},
		{NULL, NULL},
	};
	return quartz_addgloballib(Q, "io", libio);
}
