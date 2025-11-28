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
	// list of stuff
	QUARTZ_NODE_LIST,
	// null, true or false
	QUARTZ_NODE_KEYWORDCONST,
	// just a do block lmao
	QUARTZ_NODE_BLOCK,
	// first node is condition, 2nd node is body, and 3rd node is else if applicable
	QUARTZ_NODE_IF,
	// first node is condition, 2nd node is body
	QUARTZ_NODE_WHILE,
	// set a = b
	QUARTZ_NODE_ASSIGN,
	// for a, b in c
	// 2 var nodes indicating a and b
	// one node for iterated value
	// another node for the body
	QUARTZ_NODE_FOR,
	// return (possibly with child value)
	QUARTZ_NODE_RETURN,
	// a function expression.
	// all children, except for the last, are of type VARIABLE and are the parameter names.
	// the last child is the function body
	// flags is set in there for stuff
	QUARTZ_NODE_FUNCTION,
} quartz_NodeType;

typedef struct quartz_Node {
	quartz_NodeType type;
	unsigned int line;
	union {
		quartz_FunctionFlags funcflags;
	};
	const char *str;
	size_t strlen;
	struct quartz_Node **children;
	size_t childCount;
	size_t childCap;
	struct quartz_Node *next;
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
	quartz_Node *allNodes;
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

quartz_Node *quartzI_allocAST(quartz_Parser *p, quartz_NodeType type, unsigned int line, const char *str, size_t strlen);
quartz_Errno quartzI_addNodeChild(quartz_Thread *Q, quartz_Node *node, quartz_Node *child);
quartz_Errno quartzI_dumpAST(quartz_Buffer *buf, quartz_Node *node);

void quartzP_initParser(quartz_Thread *Q, quartz_Parser *p, const char *s);
void quartzP_freeParser(quartz_Parser *p);

quartz_Errno quartzP_peekToken(quartz_Parser *p, quartz_Token *t);
quartz_Errno quartzP_nextToken(quartz_Parser *p, quartz_Token *t);

// parsing code

quartz_Errno quartzP_parseExpression(quartz_Parser *p, quartz_Node *parent);
quartz_Errno quartzP_parseStatement(quartz_Parser *p, quartz_Node *parent, quartz_ParserBlockFlags flags);
quartz_Errno quartzP_parseStatementBlock(quartz_Parser *p, quartz_Node *parent, quartz_ParserBlockFlags flags);

quartz_Errno quartzP_parse(quartz_Thread *Q, quartz_Parser *p, quartz_Node *program);

#endif
