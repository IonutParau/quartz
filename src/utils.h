#ifndef QUARTZ_UTILS_H
#define QUARTZ_UTILS_H

#include "quartz.h"

typedef struct quartz_FormatData {
	size_t len;
	int min;
	int max;
	bool minZeroed;
	bool maxZeroed;
	char d;
} quartz_FormatData;

size_t quartzI_strlen(const char *s);
void quartzI_memcpy(void *dest, const void *src, size_t len);
void quartzI_memset(void *dest, unsigned char x, size_t len);
bool quartzI_strleql(const char *a, size_t alen, const char *b, size_t blen);
bool quartzI_strleqlc(const char *a, size_t alen, const char *b);
bool quartzI_streqlc(const char *a, const char *b);

// this is how string literals are processed
size_t quartzI_trueStringLen(const char *literal, size_t len);
void quartzI_trueStringWrite(char *buf, const char *literal, size_t len);

// null for error, a numeric type for valid ones.
quartz_Type quartzI_numType(const char *literal, size_t len);
quartz_Int quartzI_atoi(const char *literal, size_t len);
quartz_Real quartzI_atof(const char *literal, size_t len);
quartz_Complex quartzI_atoc(const char *literal, size_t len);

bool quartzI_iswhitespace(char c);
bool quartzI_isalpha(char c);
bool quartzI_isnum(char c);
bool quartzI_ishex(char c);
bool quartzI_isbin(char c);
bool quartzI_isoctal(char c);
// valid bases are 2, 8, 10, 16
bool quartzI_isbase(char c, int base);
bool quartzI_isident(char c);
// checks whether a byte is ASCII printable
bool quartzI_isprint(char c);

bool quartzI_isupper(char c);
bool quartzI_islower(char c);

// infers base
int quartzI_getdigit(char c);

// s should be after the %
void quartzI_parseFormatter(const char *s, quartz_FormatData *d);

#endif
