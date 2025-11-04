#include "utils.h"

size_t quartzI_strlen(const char *s) {
	size_t l = 0;
	while(*(s++)) l++;
	return l;
}

void quartzI_memcpy(void *dest, const void *src, size_t len) {
	if(dest == NULL) return;
	if(src == NULL) return;
	char *destBytes = (char *)dest;
	const char *srcBytes = (const char *)src;

	for(size_t i = 0; i < len; i++) destBytes[i] = srcBytes[i];
}

void quartzI_memset(void *dest, unsigned char x, size_t len) {
	if(dest == NULL) return;
	char *destBytes = (char *)dest;
	for(size_t i = 0; i < len; i++) destBytes[i] = x;
}

bool quartzI_strleql(const char *a, size_t alen, const char *b, size_t blen) {
	if(alen != blen) return false;
	return quartzI_strleqlc(a, alen, b);
}

bool quartzI_strleqlc(const char *a, size_t alen, const char *b) {
	for(size_t i = 0; i < alen; i++) {
		if(a[i] != b[i]) return false;
	}
	return true;
}

char *quartz_strdup(quartz_Thread *Q, const char *s) {
	size_t len = quartzI_strlen(s);
	char *buf = quartz_alloc(Q, len+1);
	if(buf == NULL) return NULL;
	buf[len] = '\0';
	return buf;
}

void quartz_strfree(quartz_Thread *Q, char *s) {
	size_t len = quartzI_strlen(s);
	quartz_free(Q, s, len+1);
}

// the ctype-like functions assume

bool quartzI_iswhitespace(char c) {
	return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == '\f';
}

bool quartzI_isalpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool quartzI_isnum(char c) {
	return c >= '0' && c <= '9';
}

bool quartzI_ishex(char c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

bool quartzI_isbin(char c) {
	return c == '0' || c == '1';
}

bool quartzI_isoctal(char c) {
	return c >= '0' && c <= '7';
}

bool quartzI_isbase(char c, int base) {
	if(base == 2) return quartzI_isbin(c);
	if(base == 8) return quartzI_isoctal(c);
	if(base == 16) return quartzI_ishex(c);
	return quartzI_isnum(c);
}

bool quartzI_isident(char c) {
	return quartzI_isalpha(c) || quartzI_isnum(c) || c == '_';
}

bool quartzI_isprint(char c) {
	return c >= ' ' && c <= '~';
}

quartz_Uint quartzI_atou(const char *s, size_t base);
quartz_Int quartzI_atoi(const char *s, size_t base);

// s should be after the %
void quartzI_parseFormatter(const char *s, quartz_FormatData *d) {
	if(s[0] == '%') {
		d->len = 1;
		d->min = 0;
		d->max = 0;
		d->d = '%';
		return;
	}
	d->len = 0;
	d->min = 0;
	d->max = 0;
	d->minZeroed = false;
	d->maxZeroed = false;
	bool isMax = false;

	while(true) {
		char c = s[d->len];
		if(c == '\0') return;
		d->len++;
		if(quartzI_isalpha(c)) {
			// we done
			d->d = c;
			return;
		} else if(quartzI_isnum(c)) {
			// shit its a num
			int n = c - '0';
			if(isMax) {
				if(d->max == 0 && n == 0) d->maxZeroed = true;
				d->max += n;
			} else {
				if(d->min == 0 && n == 0) d->minZeroed = true;
				d->min += n;
			}
		} else if(c == '*') {
			// runtime!!!
			if(isMax) d->max = -1;
			else d->min = -1;
		} else if(c == '.') {
			isMax = true;
		}
	}
}
