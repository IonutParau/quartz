#include "quartz.h"
#include "utils.h"
#include <stdarg.h>

quartz_Errno quartz_bufinit(quartz_Thread *Q, quartz_Buffer *buf, size_t cap) {
	buf->Q = Q;
	buf->len = 0;
	buf->cap = cap;
	buf->buf = quartz_alloc(Q, cap);
	return buf->buf == NULL ? QUARTZ_ENOMEM : QUARTZ_OK;
}

void quartz_bufdestroy(quartz_Buffer *buf) {
	quartz_free(buf->Q, buf->buf, buf->cap);
}

char *quartz_bufreserve(quartz_Buffer *buf, size_t amount) {
	size_t newCap = buf->cap;
	while(buf->len + amount > newCap) {
		newCap *= 2;
	}
	if(buf->cap != newCap) {
		char *newBuf = quartz_realloc(buf->Q, buf->buf, buf->cap, newCap);
		if(newBuf == NULL) return NULL;
		buf->buf = newBuf;
		buf->cap = newCap;
	}
	char *mem = buf->buf + buf->len;
	buf->len += amount;
	return mem;
}

quartz_Errno quartz_bufputc(quartz_Buffer *buf, char c) {
	char *p = quartz_bufreserve(buf, 1);
	if(p == NULL) return QUARTZ_ENOMEM;
	*p = c;
	return QUARTZ_OK;
}

quartz_Errno quartz_bufputcr(quartz_Buffer *buf, char c, size_t amount) {
	char *p = quartz_bufreserve(buf, amount);
	if(p == NULL) return QUARTZ_ENOMEM;
	quartzI_memset(p, c, amount);
	return QUARTZ_OK;
}

quartz_Errno quartz_bufputs(quartz_Buffer *buf, const char *s) {
	return quartz_bufputls(buf, s, quartzI_strlen(s));
}

quartz_Errno quartz_bufputls(quartz_Buffer *buf, const char *s, size_t len) {
	char *p = quartz_bufreserve(buf, len);
	if(p == NULL) return QUARTZ_ENOMEM;
	quartzI_memcpy(p, s, len);
	return QUARTZ_OK;
}

quartz_Errno quartz_bufputp(quartz_Buffer *buf, const void *p) {
	quartz_Errno err;
	// we assume 8 bits per byte here, not ideal
	size_t ptrSize = sizeof(p) * 2;

	err = quartz_bufputs(buf, "0x");
	if(err) return err;

	err = quartz_bufputux(buf, (quartz_Uint)p, 16, ptrSize, true);

	return QUARTZ_OK;
}

quartz_Errno quartz_bufputn(quartz_Buffer *buf, quartz_Int n) {
	return quartz_bufputnx(buf, n, 10, 0, false);
}

quartz_Errno quartz_bufputnx(quartz_Buffer *buf, quartz_Int n, size_t base, size_t minDigits, bool zeroed) {
	if(n < 0) {
		n = -n;
		quartz_Errno err = quartz_bufputc(buf, '-');
		if(err) return err;
	}

	return quartz_bufputux(buf, n, base, minDigits, zeroed);
}

quartz_Errno quartz_bufputu(quartz_Buffer *buf, quartz_Uint n) {
	return quartz_bufputux(buf, n, 10, 0, false);
}

quartz_Errno quartz_bufputux(quartz_Buffer *buf, quartz_Uint n, size_t base, size_t minDigits, bool zeroed) {
	// 2^128 <= 2^64 so fuck off Rust devs
	char digits[128];
	size_t digitlen = 0;

	while(n > 0) {
		const char *a = "0123456789abcdef";
		char d = a[n % base];
		n = n / base;
		digits[digitlen] = d;
		digitlen++;
	}

	if(digitlen == 0) {
		digits[0] = '0';
		digitlen = 1;
	}

	if(digitlen < minDigits) {
		size_t pad = minDigits - digitlen;
		quartz_Errno err = quartz_bufputcr(buf, zeroed ? '0' : ' ', pad);
		if(err) return err;
	}

	return quartz_bufputls(buf, digits, digitlen);
}

quartz_Errno quartz_bufputf(quartz_Buffer *buf, const char *fmt, ...) {
	va_list list;
	va_start(list, fmt);

	quartz_Errno err = quartz_bufputfv(buf, fmt, list);

	va_end(list);
	return err;
}

// see quartz_bufputf for format specifier details
quartz_Errno quartz_bufputfv(quartz_Buffer *buf, const char *fmt, va_list args) {
	quartz_Errno err;
	quartz_FormatData data;
	while(*fmt) {
		char c = *fmt;
		fmt++;
		if(c == '%') {
			quartzI_parseFormatter(fmt, &data);
			if(data.len == 0) break; // uhoh stinky
			// get runtime known mins and maxs
			if(data.min < 0) {
				data.min = va_arg(args, quartz_Uint);
			}
			if(data.max < 0) {
				data.max = va_arg(args, quartz_Uint);
			}
			// %f and %C are currently not implemented :pensive:
			if(data.d == 'p') {
				void *p = va_arg(args, void *);
				err = quartz_bufputp(buf, p);
				if(err) return err;
			}
			if(data.d == 's') {
				// TODO: lstring stuff
				const char *s = va_arg(args, const char *);
				err = quartz_bufputs(buf, s);
				if(err) return err;
			}
			if(data.d == 'd') {
				quartz_Int d = va_arg(args, quartz_Int);
				err = quartz_bufputnx(buf, d, 10, data.min, data.minZeroed);
				if(err) return err;
			}
			if(data.d == 'u') {
				quartz_Uint d = va_arg(args, quartz_Uint);
				err = quartz_bufputux(buf, d, 10, data.min, data.minZeroed);
				if(err) return err;
			}
			if(data.d == 'x') {
				quartz_Int d = va_arg(args, quartz_Int);
				err = quartz_bufputnx(buf, d, 16, data.min, data.minZeroed);
				if(err) return err;
			}
			if(data.d == 'X') {
				quartz_Uint d = va_arg(args, quartz_Uint);
				err = quartz_bufputux(buf, d, 16, data.min, data.minZeroed);
				if(err) return err;
			}
			if(data.d == 'o') {
				quartz_Int d = va_arg(args, quartz_Int);
				err = quartz_bufputnx(buf, d, 8, data.min, data.minZeroed);
				if(err) return err;
			}
			if(data.d == 'O') {
				quartz_Uint d = va_arg(args, quartz_Uint);
				err = quartz_bufputux(buf, d, 8, data.min, data.minZeroed);
				if(err) return err;
			}
			if(data.d == 'b') {
				quartz_Int d = va_arg(args, quartz_Int);
				err = quartz_bufputnx(buf, d, 2, data.min, data.minZeroed);
				if(err) return err;
			}
			if(data.d == 'B') {
				quartz_Uint d = va_arg(args, quartz_Uint);
				err = quartz_bufputux(buf, d, 2, data.min, data.minZeroed);
				if(err) return err;
			}
			if(data.d == 'c') {
				char c = va_arg(args, int);
				err = quartz_bufputc(buf, c);
				if(err) return err;
			}
			fmt += data.len;
		} else {
			err = quartz_bufputc(buf, c);	
			if(err) return err;
		}
	}
	return QUARTZ_OK;
}

size_t quartz_bufcount(quartz_Buffer *buf) {
	return buf->len;
}

void quartz_bufcpy(quartz_Buffer *buf, char *s) {
	quartzI_memcpy(s, buf->buf, buf->len);
}

char *quartz_bufstr(quartz_Buffer *buf, size_t *len) {
	if(len != NULL) *len = buf->len;
	char *mem = quartz_alloc(buf->Q, buf->len + 1);
	if(mem == NULL) return NULL;
	quartzI_memcpy(mem, buf->buf, buf->len);
	mem[buf->len] = '\0';
	return mem;
}

void quartz_bufshrinkto(quartz_Buffer *buf, size_t len) {
	if(buf->len > len) buf->len = len;
}

void quartz_bufreset(quartz_Buffer *buf) {
	buf->len = 0;
}
