#include "quartz.h"
#include "value.h"

static const char *quartz_libbuf_typestr = "buffer";

static quartz_Errno quartz_libbuf_buffunc(quartz_Thread *Q, void *userdata, quartz_UserOp op) {
	quartz_Buffer *buf = (quartz_Buffer *)userdata;
	if(op == QUARTZ_USER_DESTROY) {
		quartz_bufdestroy(buf);
		return QUARTZ_OK;
	}
	return QUARTZ_OK;
}

static quartz_Errno quartz_libbuf_alloc(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	if(argc == 0) {
		err = quartz_pushint(Q, 1024);
		if(err) return err;
	}
	quartz_Int num = quartz_tointeger(Q, 0, &err);
	if(err) return err;
	err = quartz_assertf(Q, num > 0, QUARTZ_ERUNTIME, "invalid buffer size: %d", num);
	if(err) return err;

	quartz_Buffer *buf;
	err = quartz_pushuserdata(Q, sizeof(quartz_Buffer), (void **)&buf, quartz_libbuf_typestr, quartz_libbuf_buffunc);
	if(err) return err;
	err = quartz_bufinit(Q, buf, num);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libbuf_len(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_Buffer *buf = quartz_touserdata(Q, 0, quartz_libbuf_typestr, &err);
	if(err) return err;
	err = quartz_pushint(Q, buf->len);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libbuf_cap(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_Buffer *buf = quartz_touserdata(Q, 0, quartz_libbuf_typestr, &err);
	if(err) return err;
	err = quartz_pushint(Q, buf->cap);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libbuf_reset(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_Buffer *buf = quartz_touserdata(Q, 0, quartz_libbuf_typestr, &err);
	if(err) return err;
	err = quartz_pushint(Q, buf->len);
	if(err) return err;
	quartz_bufreset(buf);
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libbuf_tostring(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_Buffer *buf = quartz_touserdata(Q, 0, quartz_libbuf_typestr, &err);
	if(err) return err;
	err = quartz_pushlstring(Q, buf->buf, buf->len);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libbuf_reserve(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_Buffer *buf = quartz_touserdata(Q, 0, quartz_libbuf_typestr, &err);
	if(err) return err;
	quartz_Int n = quartz_tointeger(Q, 1, &err);
	char *m = quartz_bufreserve(buf, n);
	if(m == NULL) return quartz_oom(Q);
	if(err) return err;
	err = quartz_pushint(Q, buf->cap);
	if(err) return err;
	return quartz_return(Q, -1);
}

static quartz_Errno quartz_libbuf_putc(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_Buffer *buf = quartz_touserdata(Q, 0, quartz_libbuf_typestr, &err);
	if(err) return err;
	for(size_t i = 1; i < argc; i++) {
		quartz_Int n = quartz_tointeger(Q, i, &err);
		if(err) return err;
		unsigned char c = n;
		err = quartz_assertf(Q, n == c, QUARTZ_ERUNTIME, "internal overflow: %d -> %d", n, (quartz_Int)c);
		if(err) return err;
		err = quartz_bufputc(buf, c);
		if(err) return err;
	}
	return QUARTZ_OK;
}

static quartz_Errno quartz_libbuf_puts(quartz_Thread *Q, size_t argc) {
	quartz_Errno err = QUARTZ_OK;
	quartz_Buffer *buf = quartz_touserdata(Q, 0, quartz_libbuf_typestr, &err);
	if(err) return err;
	for(size_t i = 1; i < argc; i++) {
		err = quartz_cast(Q, i, QUARTZ_TSTR);
		if(err) return err;
		size_t l;
		const char *s = quartz_tolstring(Q, i, &l, &err);
		if(err) return err;
		err = quartz_bufputls(buf, s, l);
		if(err) return err;
	}
	return QUARTZ_OK;
}

quartz_Errno quartz_openlibbuf(quartz_Thread *Q) {
	quartz_LibFunction funcs[] = {
		{"alloc", quartz_libbuf_alloc},
		{"tostring", quartz_libbuf_tostring},
		{"reset", quartz_libbuf_reset},
		{"len", quartz_libbuf_len},
		{"cap", quartz_libbuf_cap},
		{"putc", quartz_libbuf_putc},
		{"puts", quartz_libbuf_puts},

		{NULL, NULL},
	};
	return quartz_addgloballib(Q, "buf", funcs);
}

