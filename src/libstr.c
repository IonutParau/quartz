#include "quartz.h"
#include "utils.h"
#include "platform.h"

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

		err = quartz_cast(Q, -1, QUARTZ_TSTR);
		if(err) return err;

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

static quartz_Errno quartz_libstr_byte(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	size_t dataLen;
	const char *data = quartz_tolstring(Q, 0, &dataLen, &err);
	if(err) return err;

	size_t i = quartz_tointeger(Q, 1, &err);
	if(err) return err;

	err = quartz_assertf(Q, i < dataLen, QUARTZ_ERUNTIME, "index out of bounds (0..<%d, got %d)", (quartz_Int)dataLen, (quartz_Int)i);
	if(err) return err;

	unsigned char b = data[i];

	err = quartz_pushint(Q, b);
	if(err) return err;

	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libstr_char(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	if(argc == 0) {
		err = quartz_pushlstring(Q, "", 0);
		if(err) return err;
		return quartz_return(Q, -1);
	}

	bool array = quartz_typeof(Q, 0) != QUARTZ_TINT;
	size_t len = array ? quartz_len(Q, 0, &err) : argc;
	if(err) return err;

	char *buf = quartz_pushastring(Q, len, &err);
	if(err) return err;
	size_t bufIdx = quartz_gettop(Q);

	for(size_t i = 0; i < len; i++) {
		quartz_Int n;
		if(array) {
			err = quartz_pushvalue(Q, 0);
			if(err) return err;
			err = quartz_pushint(Q, i);
			if(err) return err;
			err = quartz_getindex(Q);
			if(err) return err;
			n = quartz_tointeger(Q, -1, &err);
			if(err) return err;
			err = quartz_pop(Q);
			if(err) return err;
		} else {
			n = quartz_tointeger(Q, i, &err);
			if(err) return err;
		}
		unsigned char b = n;
		err = quartz_assertf(Q, n == b, QUARTZ_ERUNTIME, "invalid integer conversion (%d -> %d)", n, (quartz_Int)b);
		if(err) return err;

		buf[i] = b;
	}

	// returns null
	return quartz_return(Q, bufIdx);
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

static quartz_Errno quartz_libstr_rep(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	size_t dataLen;
	const char *data = quartz_tolstring(Q, 0, &dataLen, &err);
	if(err) return err;
	
	size_t targetLen;
	const char *target = quartz_tolstring(Q, 1, &targetLen, &err);
	if(err) return err;
	
	size_t repLen;
	const char *rep = quartz_tolstring(Q, 2, &repLen, &err);
	if(err) return err;

	if(dataLen < targetLen || targetLen == 0) {
		return quartz_return(Q, 0);
	}
	
	size_t scanEnd = dataLen - targetLen;

	quartz_Buffer buf;
	err = quartz_bufinit(Q, &buf, dataLen);
	if(err) return err;

	size_t i = 0;

	while(i <= scanEnd) {
		if(quartzI_strleql(data + i, targetLen, target, targetLen)) {
			err = quartz_bufputls(&buf, rep, repLen);
			if(err) goto cleanup;
			i += targetLen;
		} else {
			err = quartz_bufputc(&buf, data[i]);
			if(err) goto cleanup;
			i++;
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

static quartz_Errno quartz_libstr_rev(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	size_t dataLen;
	const char *data = quartz_tolstring(Q, 0, &dataLen, &err);
	if(err) return err;

	char *buf = quartz_pushastring(Q, dataLen, &err);
	if(err) return err;

	for(size_t i = 0; i < dataLen; i++) {
		buf[dataLen - i - 1] = data[i];
	}

	// returns null
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libstr_decode(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;

	const char *fmt = quartz_tostring(Q, 0, &err);
	if(err) return err;

	size_t dataLen;
	const char *data = quartz_tolstring(Q, 1, &dataLen, &err);
	if(err) return err;

	size_t decoded = 0;
	size_t cursor = 0;
	size_t curSize = 1;
	bool firstDigit = true;
	bool littleEndian = quartzI_isLittleEndian();

	for(size_t i = 0; fmt[i] != '\0'; i++) {
		if(cursor >= dataLen) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "insufficient data");
		}
		char c = fmt[i];
		if(quartzI_iswhitespace(c)) continue;
		if(quartzI_isnum(c)) {
			int n = quartzI_getdigit(c);
			if(firstDigit) {
				curSize = n;
			} else {
				curSize = curSize * 10 + n;
			}
		} else {
			firstDigit = true;
		}
		
		if(c == '=') {
			littleEndian = quartzI_isLittleEndian();
			continue;
		}
		if(c == '<') {
			littleEndian = true;
			continue;
		}
		if(c == '>') {
			littleEndian = false;
			continue;
		}

		size_t dataLeft = dataLen - cursor;
		if(c == 'c') {
			decoded++;
			// C-string
			// remember that Quartz itself guarantees at least 1 NULL terminator
			size_t l = quartzI_strlen(data + cursor);
			err = quartz_pushlstring(Q, data + cursor, l);
			if(err) return err;
			cursor += l;
			continue;
		}
		if(c == 'f') {
			if(dataLeft < sizeof(float)) {
				return quartz_errorf(Q, QUARTZ_ERUNTIME, "insufficient data");
			}
			decoded++;
			float n;
			quartzI_memcpy((void *)&n, data + cursor, sizeof(n));
			err = quartz_pushreal(Q, n);
			if(err) return err;
			cursor += sizeof(n);
			continue;
		}
		if(c == 'F') {
			if(dataLeft < sizeof(float)) {
				return quartz_errorf(Q, QUARTZ_ERUNTIME, "insufficient data");
			}
			decoded++;
			double n;
			quartzI_memcpy((void *)&n, data + cursor, sizeof(n));
			err = quartz_pushreal(Q, n);
			if(err) return err;
			cursor += sizeof(n);
			continue;
		}
		// all of these have selectable size
		if(dataLeft < curSize) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "insufficient data");
		}

		if(c == 'p') {
			cursor += curSize;
			continue;
		}
		if(c == 'C') {
			decoded++;
			err = quartz_pushlstring(Q, data + cursor, curSize);
			if(err) return err;
			cursor += curSize;
			continue;
		}
		if(c == 'I') {
			decoded++;
			quartz_Int n = 0;
			quartz_Int m = 1;
			int base = 1 << QUARTZ_BITSPERUNIT;

			for(size_t j = 0; j < curSize; j++) {
				quartz_Int d = (unsigned char)data[cursor + j];
				if(littleEndian) {
					n += d * m;
					m *= base;
				} else {
					n *= base;
					n += d;
				}
			}

			err = quartz_pushint(Q, n);
			if(err) return err;
			cursor += curSize;
			continue;
		}
	}

	err = quartz_pushtuple(Q, decoded);
	if(err) return err;

	// returns null
	return quartz_return(Q, -1);
}

quartz_Errno quartz_openlibstr(quartz_Thread *Q) {
	quartz_LibFunction lib[] = {
		{"len", quartz_libstr_len},
		{"slice", quartz_libstr_slice},
		{"loop", quartz_libstr_loop},
		{"join", quartz_libstr_join},
		{"byte", quartz_libstr_byte},
		{"char", quartz_libstr_char},
		{"find", quartz_libstr_find},
		{"rep", quartz_libstr_rep},
		{"rev", quartz_libstr_rev},
		{"decode", quartz_libstr_decode},
		{NULL, NULL},
	};

	return quartz_addgloballib(Q, "str", lib);
}
