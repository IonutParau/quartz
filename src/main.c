#include <stdio.h>

// to get LSP to sybau
#include "quartz.h"

#include "one.c"

static quartz_Errno testCompiler(quartz_Thread *Q, const char *s) {
	quartz_Errno err;
	err = quartz_openstdlib(Q);
	if(err) return err;
	err = quartz_assertf(Q, quartz_getstacksize(Q) == 0, QUARTZ_ERUNTIME, "stack values leaked");
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

	quartz_Errno err;

	quartz_Buffer buf;
	quartz_bufinit(Q, &buf, 64);
	quartz_bufputs(&buf, "io.writeln(\"Hello, world!\")\n");
	quartz_bufputs(&buf, "io.writeln(21)\n");
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
