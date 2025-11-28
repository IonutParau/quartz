#include <stdio.h>

// to get LSP to sybau
#include "quartz.h"
#include "platform.h"

#include "one.c"

#ifdef QUARTZ_POSIX
#include <unistd.h>
#endif

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

static void printVersionAndCopyright() {
	printf("Quartz v%d.%d.%d Copyright (C) 2025 Parau Ionut Alexandru\n", QUARTZ_VER_MAJOR, QUARTZ_VER_MINOR, QUARTZ_VER_PATCH);
}

static quartz_Errno repl(quartz_Thread *Q, bool disassemble) {
	quartz_Errno err;
	const char *src = "(stdin)";
	quartz_Buffer linebuf;
	err = quartz_bufinit(Q, &linebuf, 128);
	if(err) return err;
	err = quartz_setstacksize(Q, 0);
	if(err) return err;
	// shared module
	int sharedModule = 0;
	int globals = 1;
	int chunkname = 2;
	err = quartz_pushmap(Q);
	if(err) return err;
	err = quartz_pushglobals(Q);
	if(err) return err;
	err = quartz_pushstring(Q, src);
	if(err) return err;
	err = quartz_pushvalue(Q, sharedModule);
	if(err) return err;
	err = quartz_pushstring(Q, "_M");
	if(err) return err;
	err = quartz_pushvalue(Q, sharedModule);
	if(err) return err;
	err = quartz_setindex(Q);
	if(err) return err;
	while(!feof(stdin)) {
		err = quartz_setstacksize(Q, 3);
		if(err) return err;
		quartz_bufreset(&linebuf);
		fputs("> ", stdout);
		fflush(stdout);
		while(true) {
			int c = fgetc(stdin);
			if(c == EOF || c == '\n' || c == '\4') {
				if(c == '\4') putc('\n', stdout);
				break;
			}
			err = quartz_bufputc(&linebuf, c);
			if(err) return err;
		}
		if(linebuf.len > 0) {
			err = quartz_pushlstring(Q, linebuf.buf, linebuf.len);
			if(err) return err;
			err = quartz_pushscriptx(Q, -1, chunkname, globals, sharedModule);
			if(err) {
				err = quartz_pusherror(Q);
				if(err) return err;
				err = quartz_cast(Q, -1, QUARTZ_TSTR);
				if(err) return err;
				const char *s = quartz_tostring(Q, -1, &err);
				if(err) return err;
				printf("Error: %s\n", s);
				quartz_clearerror(Q);
			} else {
				if(disassemble) {
					quartz_bufreset(&linebuf);
					err = quartz_disassemble(Q, -1, &linebuf);
					if(err) return err;
					err = quartz_pop(Q);
					if(err) return err;
					fwrite(linebuf.buf, sizeof(char), linebuf.len, stdout);
					fflush(stdout);
				} else {
					err = quartz_pushstring(Q, src);
					if(err) return err;
					// not protected, L
					err = quartz_call(Q, 1, QUARTZ_CALL_STATIC);
					if(err) return err;
				}
			}
		}
	}
	return QUARTZ_OK; // trust
}

static quartz_Errno execStdin(quartz_Thread *Q, bool disassemble) {
	quartz_Errno err;
	quartz_Buffer buf;
	err = quartz_bufinit(Q, &buf, 8192);
	if(err) return err;
	char chunk[8192];
	while(!feof(stdin)) {
		size_t len = fread(chunk, 1, sizeof(chunk), stdin);
		err = quartz_bufputls(&buf, chunk, len);
		if(err) return err;
	}
	err = quartz_pushlscript(Q, buf.buf, buf.len, "(stdin)", 7);
	quartz_bufdestroy(&buf);
	if(err) return err;
	if(disassemble) {
		// yes there can be memory leaks, but the program will exit
		// TODO: handle them anyways just as a good practice
		err = quartz_bufinit(Q, &buf, 8192);
		if(err) return err;
		err = quartz_disassemble(Q, -1, &buf);
		if(err) return err;
		err = quartz_pop(Q);
		if(err) return err;
		fwrite(buf.buf, sizeof(char), buf.len, stdout);
		fflush(stdout);
		return QUARTZ_OK;
	}
	err = quartz_pushstring(Q, "(stdin)");
	if(err) return err;
	return quartz_call(Q, 1, QUARTZ_CALL_STATIC);
}

static quartz_Errno interpreter(quartz_Thread *Q, int argc, char **argv) {
	quartz_Errno err;

	err = quartz_openstdlib(Q);
	if(err) return err;

	if(argc == 1) {
#ifdef QUARTZ_POSIX
		if(isatty(STDIN_FILENO)) {
			printVersionAndCopyright();
			return repl(Q, false);
		}
		return execStdin(Q, false);
#else
		printVersionAndCopyright();
		return repl(Q, false);
#endif
	}

	const char *helpMsg =
		QUARTZ_VERSION "\n"
		"quartz [opts] [-] [script] [...args] \n"
		"-i - Enter repl, default if no options are specified\n"
		"-v - Print version and other program information\n"
		"-h / --help - Print help message and exit\n"
		"-r <statement> - Execute <statement> as a script\n"
		"-e <expression> - Evaluate <expression> and print result\n"
		"-d - Compile and disassemble\n"
		"- - Execute stdin\n"
		"-- - Stop reading options\n"
		;

	if(argc >= 2) {
		bool interactive = false;
		bool disassemble = false;
		size_t off = 1;
		while(off < argc) {
			const char *arg = argv[off];
			if(strcmp(arg, "--") == 0) {
				off++;
				break;
			}
			if(strcmp(arg, "-i") == 0) {
				off++;
				interactive = true;
				continue;
			}
			if(strcmp(arg, "-v") == 0) {
				off++;
				printVersionAndCopyright();
				continue;
			}
			if(strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
				off++;
				fputs(helpMsg, stdout);
				fflush(stdout);
				return QUARTZ_OK;
			}
			if(strcmp(arg, "-r") == 0) {
				off++;
				const char *s = argv[off];
				if(s == NULL) {
					return quartz_errorf(Q, QUARTZ_ERUNTIME, "no statement specified");
				}
				err = quartz_pushscript(Q, s, "(cli)");
				if(err) return err;
				err = quartz_call(Q, 0, QUARTZ_CALL_STATIC);
				if(err) return err;
				off++;
				continue;
			}
			if(strcmp(arg, "-e") == 0) {
				// TODO:
				return quartz_errorf(Q, QUARTZ_ERUNTIME, "no running expressions yet");
				continue;
			}
			if(strcmp(arg, "-d") == 0) {
				off++;
				disassemble = true;
				continue;
			}
			if(strcmp(arg, "-") == 0) {
				off++;
				err = execStdin(Q, disassemble);
				if(err) return err;
				break;
			}
			break;
		}

		const char *path = argv[off];
		if(path) {
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

			if(disassemble) {
				quartz_Buffer buf;
				err = quartz_bufinit(Q, &buf, 65536);
				if(err) return quartz_erroras(Q, err);
				err = quartz_disassemble(Q, -1, &buf);
				if(err) {
					quartz_bufdestroy(&buf);
					return err;
				}
				fwrite(buf.buf, sizeof(buf.buf[0]), buf.len, stdout);
				fflush(stdout);
				return QUARTZ_OK;
			} else {
				for(size_t i = off; i < argc; i++) {
					err = quartz_pushstring(Q, argv[i]);
					if(err) return err;
				}

				err = quartz_call(Q, argc - off, QUARTZ_CALL_STATIC);
				if(err) return err;
			}
		}
		return interactive ? repl(Q, disassemble) : QUARTZ_OK;
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
		quartz_destroyThread(Q);
		return 1;
	}

	quartz_destroyThread(Q);
	return 0;
}
