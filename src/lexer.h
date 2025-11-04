#ifndef QUARTZ_LEXER_H
#define QUARTZ_LEXER_H

#include "quartz.h"

typedef enum quartz_TokenType {
	QUARTZ_TOK_EOF = 0,
	QUARTZ_TOK_COMMENT,
	QUARTZ_TOK_WHITESPACE,
	QUARTZ_TOK_NUM,
	QUARTZ_TOK_STR,
	QUARTZ_TOK_IDENT,
	QUARTZ_TOK_KEYWORD,
	QUARTZ_TOK_SYMBOL,
} quartz_TokenType;

typedef enum quartz_TokenError {
	QUARTZ_TOK_OK,
	// malformed number
	QUARTZ_TOK_EBADNUM,
	// bad character
	QUARTZ_TOK_EBADCHAR,
	// bad escape
	QUARTZ_TOK_EBADESC,
	// unfinished string
	QUARTZ_TOK_EUNFINISHEDSTR,

	QUARTZ_TOK_ECOUNT,
} quartz_TokenError;

// token is pretty big, but there is only *1* of them at once
typedef struct quartz_Token {
	quartz_TokenType tt;
	const char *s;
	size_t len;
} quartz_Token;

extern const char *quartzI_lexErrors[QUARTZ_TOK_ECOUNT];

bool quartzI_isKeyword(const char *s, size_t len);
quartz_TokenError quartzI_lexAt(const char *s, size_t off, quartz_Token *t);
size_t quartzI_countLines(const char *s, size_t off);

#endif
