#ifndef QUARTZ_UTILS_H
#define QUARTZ_UTILS_H

#include "quartz.h"

size_t quartzI_strlen(const char *s);
void quartzI_memcpy(void *dest, const void *src, size_t len);
void quartzI_memset(void *dest, unsigned char x, size_t len);
bool quartzI_strleql(const char *a, size_t alen, const char *b, size_t blen);
bool quartzI_strleqlc(const char *a, size_t alen, const char *b);

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

#endif
