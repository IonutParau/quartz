#include "quartz.h"
#include "utils.h"

static quartz_Errno quartz_libstr_len(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	size_t len;
	quartz_tolstring(Q, 0, &len, &err);
	if(err) return err;

	err = quartz_pushint(Q, len);
	if(err) return err;

	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libstr_slice(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	size_t len;
	const char *s = quartz_tolstring(Q, 0, &len, &err);
	if(err) return err;

	quartz_Int start = 0;
	quartz_Int end = len;

	if(quartz_typeof(Q, 1) == QUARTZ_TINT) {
		start = quartz_tointeger(Q, 1, &err);
		if(err) return err;
	}
	
	if(quartz_typeof(Q, 2) == QUARTZ_TINT) {
		end = quartz_tointeger(Q, 2, &err);
		if(err) return err;
	}

	if(start < 0) start += len;
	if(end < 0) end += len;

	if(start < 0) start = 0; // this is if it is still negative
	if(end > len) end = len;
	if(end <= start) {
		err = quartz_pushlstring(Q, "", 0);
		if(err) return err;
		return quartz_return(Q, -1);
	}

	err = quartz_pushlstring(Q, s + start, end - start + 1);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libstr_loop(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	size_t len;
	const char *s = quartz_tolstring(Q, 0, &len, &err);
	if(err) return err;

	quartz_Int n = quartz_tointeger(Q, 1, &err);
	if(err) return err;

	if(n < 0) {
		err = quartz_pushlstring(Q, "", 0);
		if(err) return err;
		return quartz_return(Q, -1);
	}

	// TODO: check overflow

	char *m = quartz_pushastring(Q, len * n, &err);
	if(err) return err;

	for(size_t i = 0; i < n; i++) {
		quartzI_memcpy(m + i * len, s, len);
	}

	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libstr_join(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	quartz_Buffer buf;
	err = quartz_bufinit(Q, &buf, 1024);
	if(err) return err;

	size_t len = quartz_len(Q, 0, &err);
	if(err) goto cleanup;

	// sep
	size_t sepLen = 0;
	const char *sep = "";

	if(quartz_typeof(Q, 1) != QUARTZ_TNULL) {
		sep = quartz_tolstring(Q, 1, &sepLen, &err);
		if(err) goto cleanup;
	}

	for(size_t i = 0; i < len; i++) {
		err = quartz_pushvalue(Q, 0);
		if(err) goto cleanup;
		err = quartz_pushint(Q, i);
		if(err) goto cleanup;
		err = quartz_getindex(Q);
		if(err) goto cleanup;

		size_t partlen;
		const char *part = quartz_tolstring(Q, -1, &partlen, &err);
		if(err) goto cleanup;

		err = quartz_bufputls(&buf, part, partlen);
		if(err) goto cleanup;
		if(i < len-1) {
			err = quartz_bufputls(&buf, sep, sepLen);
			if(err) goto cleanup;
		}
	}

	err = quartz_pushlstring(Q, buf.buf, buf.len);
	if(err) goto cleanup;
	
	err = quartz_return(Q, -1);
	if(err) goto cleanup;

cleanup:
	quartz_bufdestroy(&buf);
	return err;
}

static quartz_Errno quartz_libstr_find(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	size_t dataLen;
	const char *data = quartz_tolstring(Q, 0, &dataLen, &err);
	if(err) return err;
	
	size_t targetLen;
	const char *target = quartz_tolstring(Q, 1, &targetLen, &err);
	if(err) return err;

	if(dataLen < targetLen) return QUARTZ_OK;
	
	size_t scanEnd = dataLen - targetLen;

	for(size_t i = 0; i <= scanEnd; i++) {
		if(quartzI_strleql(data + i, targetLen, target, targetLen)) {
			err = quartz_pushint(Q, i);
			if(err) return err;
			return quartz_return(Q, -1);
		}
	}

	// returns null
	return QUARTZ_OK;
}

quartz_Errno quartz_openlibstr(quartz_Thread *Q) {
	quartz_LibFunction lib[] = {
		{"len", quartz_libstr_len},
		{"slice", quartz_libstr_slice},
		{"loop", quartz_libstr_loop},
		{"join", quartz_libstr_join},
		{"find", quartz_libstr_find},
		{NULL, NULL},
	};

	return quartz_addgloballib(Q, "str", lib);
}
