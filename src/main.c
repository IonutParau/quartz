#include <stdio.h>

// to get LSP to sybau
#include "quartz.h"

#include "one.c"

static quartz_Errno testPrint(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_File *_stdout = quartz_fstdfile(Q, QUARTZ_STDOUT);
	if(_stdout == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stdout does not exist");
	}
	size_t written = 0;
	for(size_t i = 0; i < argc; i++) {
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

static quartz_Errno testStuff(quartz_Thread *Q) {
	quartz_Errno err;

	// make io table
	err = quartz_pushmap(Q);
	if(err) return err;
	size_t io = quartz_gettop(Q);

	// set io.writeln
	err = quartz_pushvalue(Q, io);
	if(err) return err;
	err = quartz_pushstring(Q, "writeln");
	if(err) return err;
	err = quartz_pushcfunction(Q, testPrint);
	if(err) return err;
	err = quartz_setindex(Q);

	// set global
	err = quartz_pushglobals(Q);
	if(err) return err;
	err = quartz_pushstring(Q, "io");
	if(err) return err;
	err = quartz_pushvalue(Q, io);
	if(err) return err;
	err = quartz_setindex(Q);
	if(err) return err;

	// since this isn't invoked as a function, we manually pop
	return quartz_popn(Q, quartz_getstacksize(Q));
}

static quartz_Errno testCompiler(quartz_Thread *Q, const char *s) {
	quartz_Errno err;
	err = quartz_fopenstdio(Q);
	if(err) return err;
	err = quartz_pushscript(Q, s, "test script");
	if(err) return err;
	return quartz_call(Q, 0, QUARTZ_CALL_STATIC);
}

int main(int argc, char **argv) {
	size_t cSize = quartz_sizeOfContext();
	char cMem[cSize];
	quartz_Context *ctx = (quartz_Context*)(void *)cMem;
	quartz_initContext(ctx, NULL);
	quartz_Thread *Q = quartz_newThread(ctx);
	quartz_Errno err = testStuff(Q);
	if(err) {
		quartz_pusherror(Q);
		printf("Error: %s\n", quartz_tostring(Q, -1, &err));
		return 1;
	}

	quartz_Buffer buf;
	quartz_bufinit(Q, &buf, 64);
	quartz_bufputs(&buf, "io.writeln(\"Hello, world!\")\n");
	// commented out because we will handle compiling these later
	//quartz_bufputs(&buf, "# comment\n");
	//quartz_bufputs(&buf, "local x = 53\n");
	//quartz_bufputs(&buf, "local y = (21 + 2i)\n");
	//quartz_bufputs(&buf, "local z = \"hi\"\n");
	//quartz_bufputs(&buf, "io[\"writeln\"](x + y, z)\n");

	char *s = quartz_bufstr(&buf, NULL);

	err = testCompiler(Q, s);
	if(err) {
		quartz_pusherror(Q);
		printf("Error: %s\n", quartz_tostring(Q, -1, &err));
		return 1;
	}

	quartz_bufdestroy(&buf);
	quartz_strfree(Q, s);
	printf("Peak Memory Usage: %zu\n", quartz_gcPeak(Q));
	quartz_destroyThread(Q);
	return 0;
}
