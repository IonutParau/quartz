#include "quartz.h"
#include "utils.h"

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

quartz_Errno quartz_bufputn(quartz_Buffer *buf, quartz_Int n) {
	return quartz_bufputnx(buf, n, 10, 0, false);
}

quartz_Errno quartz_bufputnx(quartz_Buffer *buf, quartz_Int n, size_t base, size_t minDigits, bool zeroed) {
	// 2^128 <= 2^64 so fuck off Rust devs
	char digits[128];
	size_t digitlen = 0;
	
	if(n < 0) {
		n = -n;
		quartz_Errno err = quartz_bufputc(buf, '-');
		if(err) return err;
	}

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
