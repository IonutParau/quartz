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

static quartz_Errno interpreter(quartz_Thread *Q, int argc, char **argv) {
	quartz_Errno err;

	err = quartz_openstdlib(Q);
	if(err) return err;

	if(argc == 1) {
		// repl
		printf("Quartz v%d.%d.%d Copyright (C) 2025 Parau Ionut Alexandru\n", QUARTZ_VER_MAJOR, QUARTZ_VER_MINOR, QUARTZ_VER_PATCH);
		char *line = NULL;
		size_t linebuflen = 0;
		const char *src = "stdin";
		size_t srclen = strlen(src);
		while(true) {
			fputs("> ", stdout);
			fflush(stdout);
			ssize_t linelen = getline(&line, &linebuflen, stdin);
			if(linelen < 0) {
				free(line);
				fputs("\n", stdout);
				return QUARTZ_OK; // trust
			}
			err = quartz_pushlscript(Q, line, linelen, src, srclen);
			if(err) return err;
			err = quartz_pushlstring(Q, src, srclen);
			if(err) return err;
			// not protected, L
			err = quartz_call(Q, 1, QUARTZ_CALL_STATIC);
			if(err) return err;
		}
	}

	if(argc >= 2) {
		// TODO: run script
		const char *path = argv[1];

		FILE *f = fopen(path, "r");
		if(f == NULL) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s", strerror(errno));
		}

		fseek(f, 0, SEEK_END);
		size_t fileSize = ftell(f);
		fseek(f, 0, SEEK_SET);

		char *buf = malloc(fileSize+1);
		size_t read = 0;
		while(read < fileSize) {
			size_t fresh = fread(buf + read, 1, fileSize - read, f);
			read += fresh;
		}
		buf[fileSize] = '\0';

		err = quartz_pushscript(Q, buf, path);
		free(buf);
		if(err) return err;

		for(size_t i = 1; i < argc; i++) {
			err = quartz_pushstring(Q, argv[i]);
			if(err) return err;
		}

		return quartz_call(Q, argc - 1, QUARTZ_CALL_STATIC);
	}

	return QUARTZ_OK;
}

int main(int argc, char **argv) {
	size_t cSize = quartz_sizeOfContext();
	char cMem[cSize];
	quartz_Context *ctx = (quartz_Context*)(void *)cMem;
	quartz_initContext(ctx, NULL);
	quartz_Thread *Q = quartz_newThread(ctx);

	quartz_Errno err;

	err = interpreter(Q, argc, argv);
	if(err) {
		printf("ERROR!\n");
		quartz_pusherror(Q);
		printf("Error: %s\n", quartz_tostring(Q, -1, &err));
		return 1;
	}

	printf("Peak Memory Usage: %zu\n", quartz_gcPeak(Q));
	quartz_destroyThread(Q);
	return 0;
}
