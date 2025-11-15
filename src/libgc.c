#include "quartz.h"

quartz_Errno quartz_libgc_count(quartz_Thread *Q, size_t argc) {
	quartz_Errno err;
	err = quartz_pushint(Q, quartz_gcCount(Q));
	if(err) return err;
	return quartz_return(Q, -1);
}

quartz_Errno quartz_libgc_peak(quartz_Thread *Q, size_t argc) {
	quartz_Errno err;
	err = quartz_pushint(Q, quartz_gcPeak(Q));
	if(err) return err;
	return quartz_return(Q, -1);
}

quartz_Errno quartz_libgc_target(quartz_Thread *Q, size_t argc) {
	quartz_Errno err;
	err = quartz_pushint(Q, quartz_gcTarget(Q));
	if(err) return err;
	return quartz_return(Q, -1);
}

quartz_Errno quartz_libgc_collect(quartz_Thread *Q, size_t argc) {
	quartz_Errno err;
	quartz_gc(Q);
	err = quartz_pushint(Q, quartz_gcCount(Q));
	if(err) return err;
	return quartz_return(Q, -1);
}

quartz_Errno quartz_openlibgc(quartz_Thread *Q) {
	quartz_LibFunction funcs[] = {
		{"count", quartz_libgc_count},
		{"peak", quartz_libgc_peak},
		{"target", quartz_libgc_target},
		{"collect", quartz_libgc_collect},

		{NULL, NULL},
	};
	return quartz_addgloballib(Q, "gc", funcs);
}
