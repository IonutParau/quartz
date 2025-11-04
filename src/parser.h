#ifndef QUARTZ_PARSER_H
#define QUARTZ_PARSER_H

#include "quartz.h"
#include "lexer.h"
#include "value.h"

typedef enum quartz_NodeType {
	QUARTZ_NODE_PROGRAM = 0,
	QUARTZ_NODE_NUM,
	QUARTZ_NODE_STR,
	// str + strlen is the varname, children[0] is the value, if present
	QUARTZ_NODE_LOCAL,
	// str + strlen is the varname
	QUARTZ_NODE_VARIABLE,
	// children[0] is value, children[1] is index
	QUARTZ_NODE_INDEX,
	// children[0] is value, str + strlen is field
	QUARTZ_NODE_FIELD,
	// children[0] is function, everything after is args
	QUARTZ_NODE_CALL,
	// some operator, stored in str + strlen. Where its binary or unary is based off childCount.
	QUARTZ_NODE_OP,
} quartz_NodeType;

typedef struct quartz_Node {
	quartz_NodeType type;
	size_t line;
	const char *str;
	size_t strlen;
	struct quartz_Node **children;
	size_t childCount;
	size_t childCap;
} quartz_Node;

typedef enum quartz_ParserError {
	QUARTZ_PARSE_OK = 0,
	// Lexer error (in lErr)
	QUARTZ_PARSE_ELEX,
	// Internal error (in intErrno)
	QUARTZ_PARSE_EINT,
	// expression expected
	QUARTZ_PARSE_EEXPR,
	// statement expected
	QUARTZ_PARSE_ESTMT,
	// break or continue outside of loop
	QUARTZ_PARSE_ENOLOOP,
	// bad token (str in tokExpected)
	QUARTZ_PARSE_EBADTOK,

	QUARTZ_PARSE_ECOUNT,
} quartz_ParserError;

typedef struct quartz_Parser {
	quartz_Thread *Q;
	const char *s;
	size_t off;
	size_t curline;
	quartz_Token curToken;
	size_t errloc; // error line
	quartz_ParserError pErr;
	union {
		quartz_Errno intErrno;
		quartz_TokenError lexErr;
		const char *tokExpected;
	};
} quartz_Parser;

typedef enum quartz_ParserBlockFlags {
	// changes block terminator to EOF and } to unexpected
	QUARTZ_PBLOCK_TOPSCOPE = 1<<0,
	// allows break and continue
	QUARTZ_PBLOCK_LOOP = 1<<1,
} quartz_ParserBlockFlags;

quartz_Node *quartzI_allocAST(quartz_Thread *Q, quartz_NodeType type, unsigned int line, const char *str, size_t strlen);
void quartzI_freeAST(quartz_Thread *Q, quartz_Node *node);
quartz_Errno quartzI_addNodeChild(quartz_Thread *Q, quartz_Node *node, quartz_Node *child);
quartz_Errno quartzI_addNodeChildOrFree(quartz_Thread *Q, quartz_Node *node, quartz_Node *child);
quartz_Errno quartzI_dumpAST(quartz_Buffer *buf, quartz_Node *node);

// make test ast
quartz_Node *quartzI_testAST(quartz_Thread *Q);

void quartzP_initParser(quartz_Thread *Q, quartz_Parser *p, const char *s);

quartz_Errno quartzP_peekToken(quartz_Parser *p, quartz_Token *t);
quartz_Errno quartzP_nextToken(quartz_Parser *p, quartz_Token *t);

// parsing code

quartz_Errno quartzP_parseExpression(quartz_Parser *p, quartz_Node *parent);
quartz_Errno quartzP_parseStatement(quartz_Parser *p, quartz_Node *parent, quartz_ParserBlockFlags flags);
quartz_Errno quartzP_parseStatementBlock(quartz_Parser *p, quartz_Node *parent, quartz_ParserBlockFlags flags);

quartz_Errno quartzP_parse(quartz_Thread *Q, quartz_Parser *p, quartz_Node *program);

#endif
