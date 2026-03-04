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

typedef enum qrtz_LexerError {
	QRTZ_LEX_OK = 0,
	// unknown character
	QRTZ_LEX_EBADCHAR,
	// malformed number
	QRTZ_LEX_EBADNUM,
	// bad string escape
	QRTZ_LEX_EBADSTR,
	// unfinished string
	QRTZ_LEX_EUNFINISHEDSTR,
} qrtz_LexerError;

typedef struct qrtz_Token {
	qrtz_TokenType tt;
	size_t off;
	const char *s;
	size_t len;
} qrtz_Token;

// s must be NULl-terminated
// it finds the token at an offset. Can be used to iterate the tokens.
qrtz_LexerError qrtz_lexAt(const char *s, size_t off, qrtz_Token *t);

typedef enum qrtz_NodeType {
	QRTZ_NODE_PROGRAM,
} qrtz_NodeType;

typedef struct qrtz_Node {
	struct qrtz_Node *next;
	qrtz_NodeType type;
	qrtz_Node **nodes;
	size_t nodelen;
	size_t nodecap;
} qrtz_Node;

typedef struct qrtz_Parser {
	qrtz_Node *nodes;
	const char *s;
	size_t off;
	qrtz_Node *program;
} qrtz_Parser;

// init a parser over some text
void qrtzP_initParser(qrtz_Parser *parser, const char *s);

#endif
