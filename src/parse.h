#ifndef QUARTZ_PARSE
#define QUARTZ_PARSE

#include <stddef.h>

typedef enum qrtz_TokenType {
	QRTZ_TT_EOF = 0,
	QRTZ_TT_STR,
	QRTZ_TT_IDENT,
	QRTZ_TT_KEYWORD,
	QRTZ_TT_NUMBER,
	QRTZ_TT_SYMBOL,
	QRTZ_TT_COMMENT,
	QRTZ_TT_WHITESPACE,
} qrtz_TokenType;

typedef struct qrtz_Token {
	qrtz_TokenType tt;
	size_t off;
	const char *s;
	size_t len;
} qrtz_Token;

#endif
