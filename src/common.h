#ifndef QUARTZ_VAL
#define QUARTZ_VAL

#ifndef QUARTZ_CHARBITS
#ifdef __CHAR_BIT__
#define QUARTZ_CHARBITS __CHAR_BIT__
#else
#define QUARTZ_CHARBITS 8
#endif
#endif

#include <stddef.h>
#include "quartz.h"

bool qrtz_sizeOverflows(size_t a, size_t b);
size_t qrtz_strlen(const char *s);
char *qrtz_strdup(qrtz_Context *ctx, const char *s);
void qrtz_strfree(qrtz_Context *ctx, char *s);
void qrtz_memcpy(void *dest, const void *src, size_t len);

#endif
