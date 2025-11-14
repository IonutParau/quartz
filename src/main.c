#include <stdio.h>

// to get LSP to sybau
#include "quartz.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"

#define QUARTZ_USE_LIBC
#include "one.c"

static quartz_Errno testPrint(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	for(size_t i = 0; i < argc; i++) {
		const char *s = quartz_tostring(Q, i, &err);
		if(err) return err;
		printf("%s", s);
	}
	printf("\n");
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

static quartz_Errno testCompiler(quartz_Thread *Q, quartz_Node *ast) {
	quartz_Compiler c;
	c.Q = NULL;
	quartz_Errno err = QUARTZ_OK;
	err = quartzC_initCompiler(Q, &c);
	if(err) goto cleanup;
	err = quartzC_compileProgram(&c, ast);
	if(err) goto cleanup;
	quartz_String *s = quartzI_newCString(Q, "test script");
	if(s == NULL) {
		err = quartz_oom(Q);
		goto cleanup;
	}
	quartz_Function *f = quartzC_toFunctionAndFree(&c, s, Q->gState->globals);
	c.Q = NULL;
	if(f == NULL) {
		err = quartz_oom(Q);
		goto cleanup;
	}
	err = quartzI_pushRawValue(Q, (quartz_Value) {.type = QUARTZ_VOBJ, .obj = &f->obj});
	if(err) goto cleanup;
	err = quartz_call(Q, 0, QUARTZ_CALL_STATIC);
	if(err) goto cleanup;
cleanup:
	if(err && c.Q == Q) quartzC_freeFailingCompiler(c);
	return err;
}

int main(int argc, char **argv) {
	size_t cSize = quartz_sizeOfContext();
	char cMem[cSize];
	quartz_Context *ctx = (quartz_Context*)(void *)cMem;
	quartz_initContext(ctx, NULL);
	quartz_Thread *Q = quartz_newThread(ctx);
	printf("Thread: %p\n", Q);
	printf("Memory Usage: %zu\n", quartz_gcCount(Q));

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

	printf("Code:\n%s\n", s);
	quartz_Token tok;
	size_t off = 0;
	while(true) {
		quartz_TokenError err = quartzI_lexAt(s, off, &tok);
		if(err) {
			printf("Error:%zu: %s\n", 1+quartzI_countLines(s, off), quartzI_lexErrors[err]);
			return 1;
		}
		printf("0x%04zX %d %zu %.*s\n", off, tok.tt, tok.len, (int)tok.len, tok.s);
		off += tok.len;
		if(tok.tt == QUARTZ_TOK_EOF) break;
	}

	quartz_Node *ast = quartzI_allocAST(Q, QUARTZ_NODE_PROGRAM, 0, "", 0);
	quartz_bufreset(&buf);
	quartz_Parser p;
	quartzP_initParser(Q, &p, s);
	if(quartzP_parse(Q, &p, ast)) {
		printf("Error %zu: ", p.errloc);
		if(p.pErr == QUARTZ_PARSE_ELEX) {
			printf("%s", quartzI_lexErrors[p.lexErr]);
		} else if(p.pErr == QUARTZ_PARSE_EINT) {
			printf("internal error");
		} else if(p.pErr == QUARTZ_PARSE_EEXPR) {
			printf("expected expression");
		} else if(p.pErr == QUARTZ_PARSE_ESTMT) {
			printf("expected statement");
		} else if(p.pErr == QUARTZ_PARSE_EBADTOK) {
			printf("expected %s", p.tokExpected);
		}
		printf("\n");
	}
	quartzI_dumpAST(&buf, ast);

	printf("%.*s\n", (int)buf.len, buf.buf);
	
	err = testCompiler(Q, ast);
	if(err) {
		quartz_pusherror(Q);
		printf("Error: %s\n", quartz_tostring(Q, -1, &err));
		return 1;
	}

	quartzI_freeAST(Q, ast);
	quartz_bufdestroy(&buf);
	quartz_strfree(Q, s);
	printf("Peak Memory Usage: %zu\n", quartz_gcPeak(Q));
	quartz_destroyThread(Q);
	return 0;
}
