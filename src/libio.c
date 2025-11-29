#include "filedesc.h"
#include "quartz.h"

static quartz_File *quartz_libio_getGlobalFile(quartz_Thread *Q, const char *field, quartz_Errno *err) {
	*err = quartz_getglobal(Q, "io");
	if(*err) return NULL;
	*err = quartz_pushstring(Q, field);
	if(*err) return NULL;
	*err = quartz_getindex(Q);
	if(*err) return NULL;

	quartz_fd *fd = quartzFD_tofd(Q, -1, err);
	if(*err) return NULL;

	return quartzFD_unwrapfd(Q, fd);
}

static quartz_Errno quartz_libio_writeln(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_File *_stdout = quartz_libio_getGlobalFile(Q, "stdout", &err);
	if(err) return err;
	if(_stdout == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stdout is closed");
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

static quartz_Errno quartz_libio_write(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_File *_stdout = quartz_libio_getGlobalFile(Q, "stdout", &err);
	if(err) return err;
	if(_stdout == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stdout is closed");
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
	if(!err) {
		err = quartz_pushint(Q, written);
		if(err) return err;
		err = quartz_return(Q, -1);
	}
	return err;
}

static quartz_Errno quartz_libio_ewriteln(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_File *_stderr = quartz_libio_getGlobalFile(Q, "stderr", &err);
	if(err) return err;
	if(_stderr == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stderr is closed");
	}
	size_t written = 0;
	for(size_t i = 0; i < argc; i++) {
		err = quartz_cast(Q, i, QUARTZ_TSTR);
		if(err) return err;
		size_t len;
		const char *s = quartz_tolstring(Q, i, &len, &err);
		if(err) return err;
		err = quartz_fwrite(Q, _stderr, s, &len);
		if(err) return err;
		written += len;
	}
	size_t len = 1;
	err = quartz_fwrite(Q, _stderr, "\n", &len);
	if(!err) {
		written += len;
		err = quartz_pushint(Q, written);
		if(err) return err;
		err = quartz_return(Q, -1);
	}
	return err;
}

static quartz_Errno quartz_libio_ewrite(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_File *_stderr = quartz_libio_getGlobalFile(Q, "stderr", &err);
	if(err) return err;
	if(_stderr == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stderr is closed");
	}
	size_t written = 0;
	for(size_t i = 0; i < argc; i++) {
		err = quartz_cast(Q, i, QUARTZ_TSTR);
		if(err) return err;
		size_t len;
		const char *s = quartz_tolstring(Q, i, &len, &err);
		if(err) return err;
		err = quartz_fwrite(Q, _stderr, s, &len);
		if(err) return err;
		written += len;
	}
	if(!err) {
		err = quartz_pushint(Q, written);
		if(err) return err;
		err = quartz_return(Q, -1);
	}
	return err;
}

static quartz_Errno quartz_libio_flush(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_File *_stdout = quartz_libio_getGlobalFile(Q, "stdout", &err);
	if(err) return err;
	if(_stdout == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stdout is closed");
	}
	err = quartz_fflush(Q, _stdout);
	if(err) return err;
	
	quartz_File *_stderr = quartz_libio_getGlobalFile(Q, "stderr", &err);
	if(err) return err;
	if(_stdout == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stderr is closed");
	}
	err = quartz_fflush(Q, _stdout);
	if(err) return err;

	return QUARTZ_OK;
}

static quartz_Errno quartz_libio_read(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_File *_stdin = quartz_libio_getGlobalFile(Q, "stdin", &err);
	if(err) return err;
	if(_stdin == NULL) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "stdin is closed");
	}

	if(quartz_typeof(Q, 0) == QUARTZ_TINT) {
		// read N bytes
		quartz_Int n = quartz_tointeger(Q, 0, &err);
		if(err) return err;

		err = quartz_assertf(Q, n > 0, QUARTZ_ERUNTIME, "bad read size: %d", n);
		if(err) return err;

		char *buf = quartz_alloc(Q, n);
		if(buf == NULL) return quartz_oom(Q);
		size_t buflen = n;

		err = quartz_fread(Q, _stdin, buf, &buflen);
		if(err) goto readn_cleanup;

		if(buflen > 0) {
			err = quartz_pushlstring(Q, buf, buflen);
			if(err) goto readn_cleanup;
		} else {
			err = quartz_pushnull(Q);
			if(err) goto readn_cleanup;
		}

		err = quartz_return(Q, -1);

readn_cleanup:
		quartz_free(Q, buf, n);
		return err;
	}

	// empty strings still have a NULL terminator
	const char *s = quartz_tostring(Q, 0, &err);
	if(err) return err;

	if(s[0] == 'l') {
		quartz_Buffer buf;
		err = quartz_bufinit(Q, &buf, 1024);
		if(err) goto readl_cleanup;

		err = quartz_freadline(Q, _stdin, &buf);
		if(err) goto readl_cleanup;

		if(buf.len > 0) {
			err = quartz_pushlstring(Q, buf.buf, buf.len);
			if(err) goto readn_cleanup;
		} else {
			err = quartz_pushnull(Q);
			if(err) goto readn_cleanup;
		}
		
		err = quartz_return(Q, -1);
		if(err) goto readl_cleanup;
readl_cleanup:
		quartz_bufdestroy(&buf);
		return err;
	}
	
	if(s[0] == 'a') {
		quartz_Buffer buf;
		err = quartz_bufinit(Q, &buf, 1024);
		if(err) goto reada_cleanup;

		err = quartz_freadall(Q, _stdin, &buf);
		if(err) goto reada_cleanup;

		if(buf.len > 0) {
			err = quartz_pushlstring(Q, buf.buf, buf.len);
			if(err) goto readn_cleanup;
		} else {
			err = quartz_pushnull(Q);
			if(err) goto readn_cleanup;
		}
		
		err = quartz_return(Q, -1);
		if(err) goto reada_cleanup;
reada_cleanup:
		quartz_bufdestroy(&buf);
		return err;
	}

	return quartz_errorf(Q, QUARTZ_ERUNTIME, "bad read mode: %s", s);
}

static quartz_Errno quartz_libio_setupStdf(quartz_Thread *Q, const char *name, quartz_StdFile stdf) {
	quartz_Errno err;
	err = quartz_getglobal(Q, "io");
	if(err) return err;
	err = quartz_pushstring(Q, name);
	if(err) return err;
	quartz_fd *f;
	err = quartzFD_pushfd(Q, &f);
	if(err) return err;
	f->f = NULL;
	f->stdf = stdf;
	err = quartz_setindex(Q);
	if(err) return err;

	return QUARTZ_OK;
}

quartz_Errno quartz_openlibio(quartz_Thread *Q) {
	quartz_Errno err = quartz_fopenstdio(Q);
	if(err) return err;
	quartz_LibFunction libio[] = {
		{"flush", quartz_libio_flush},
		{"write", quartz_libio_write},
		{"writeln", quartz_libio_writeln},
		{"ewrite", quartz_libio_ewrite},
		{"ewriteln", quartz_libio_ewriteln},
		{"read", quartz_libio_read},
		{NULL, NULL},
	};
	err = quartz_addgloballib(Q, "io", libio);
	if(err) return err;

	err = quartz_libio_setupStdf(Q, "stdin", QUARTZ_STDIN);
	if(err) return err;

	err = quartz_libio_setupStdf(Q, "stdout", QUARTZ_STDOUT);
	if(err) return err;

	err = quartz_libio_setupStdf(Q, "stderr", QUARTZ_STDERR);
	if(err) return err;

	return QUARTZ_OK;
}
