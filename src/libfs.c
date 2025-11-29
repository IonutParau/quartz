#include "filedesc.h"
#include "quartz.h"

quartz_Errno quartz_libfs_open(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	size_t len;
	const char *s = quartz_tolstring(Q, 0, &len, &err);
	if(err) return err;
	
	quartz_FileMode mode = QUARTZ_FILE_READABLE;

	quartz_File *f = quartz_fopen(Q, s, len, mode, &err);
	if(err) return err;

	quartz_fd *fd;

	err = quartzFD_pushfd(Q, &fd);
	if(err) {
		quartz_fclose(Q, f);
		return err;
	}

	fd->f = f;
	fd->stdf = QUARTZ_STDFILE_COUNT;

	return quartz_return(Q, -1);
}

quartz_Errno quartz_libfs_read(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	quartz_fd *fd = quartzFD_tofd(Q, 0, &err);
	if(err) return err;

	quartz_File *f = quartzFD_unwrapfd(Q, fd);
	if(f == NULL) return QUARTZ_OK; // closed
	
	if(quartz_typeof(Q, 1) == QUARTZ_TINT) {
		// read N bytes
		quartz_Int n = quartz_tointeger(Q, 1, &err);
		if(err) return err;

		err = quartz_assertf(Q, n > 0, QUARTZ_ERUNTIME, "bad read size: %d", n);
		if(err) return err;

		char *buf = quartz_alloc(Q, n);
		if(buf == NULL) return quartz_oom(Q);
		size_t buflen = n;

		err = quartz_fread(Q, f, buf, &buflen);
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
	const char *s = quartz_tostring(Q, 1, &err);
	if(err) return err;

	if(s[0] == 'l') {
		quartz_Buffer buf;
		err = quartz_bufinit(Q, &buf, 1024);
		if(err) goto readl_cleanup;

		err = quartz_freadline(Q, f, &buf);
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

		err = quartz_freadall(Q, f, &buf);
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

	return err;
}

quartz_Errno quartz_libfs_write(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	quartz_fd *fd = quartzFD_tofd(Q, 0, &err);
	if(err) return err;

	quartz_File *f = quartzFD_unwrapfd(Q, fd);
	if(f == NULL) return QUARTZ_OK; // closed
	
	size_t written = 0;

	for(size_t i = 1; i < argc; i++) {
		err = quartz_cast(Q, i, QUARTZ_TSTR);
		if(err) return err;

		size_t len;
		const char *s = quartz_tolstring(Q, i, &len, &err);
		if(err) return err;

		err = quartz_fwrite(Q, f, s, &len);
		if(err) return err;

		written += len;
	}

	err = quartz_pushint(Q, written);
	if(err) return err;
	return quartz_return(Q, -1);
}

quartz_Errno quartz_libfs_flush(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	for(size_t i = 0; i < argc; i++) {
		quartz_fd *fd = quartzFD_tofd(Q, i, &err);
		if(err) return err;

		quartz_File *f = quartzFD_unwrapfd(Q, fd);

		if(f == NULL) continue; // closed
		err = quartz_fflush(Q, f);
		if(err) return err;
	}

	return QUARTZ_OK;
}

quartz_Errno quartz_libfs_close(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	for(size_t i = 0; i < argc; i++) {
		quartz_fd *fd = quartzFD_tofd(Q, i, &err);
		if(err) return err;

		err = quartzFD_closefd(Q, fd);
		if(err) return err;
	}

	return QUARTZ_OK;
}

quartz_Errno quartz_openlibfs(quartz_Thread *Q) {
	quartz_LibFunction lib[] = {
		{"open", quartz_libfs_open},
		{"read", quartz_libfs_read},
		{"write", quartz_libfs_write},
		{"flush", quartz_libfs_flush},
		{"close", quartz_libfs_close},
		{NULL, NULL},
	};
	return quartz_addgloballib(Q, "fs", lib);
}
