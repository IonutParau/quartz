#include "quartz.h"
#include "lexer.h"
#include "utils.h"
#include "parser.h"

quartz_Node *quartzI_allocAST(quartz_Parser *p, quartz_NodeType type, unsigned int line, const char *str, size_t strlen) {
	quartz_Node *n = quartz_alloc(p->Q, sizeof(*n));
	if(n == NULL) return NULL;
	n->type = type;
	n->line = line;
	n->str = str;
	n->strlen = strlen;
	n->childCount = 0;
	n->childCap = 0;
	n->children = NULL;
	n->next = p->allNodes;
	p->allNodes = n;
	return n;
}

static void quartzP_freeAST(quartz_Thread *Q, quartz_Node *node) {
	quartz_free(Q, node->children, sizeof(node->children[0]) * node->childCap);
	quartz_free(Q, node, sizeof(*node));
}

quartz_Errno quartzI_addNodeChild(quartz_Thread *Q, quartz_Node *node, quartz_Node *child) {
	if(node->childCap == 0) {
		node->children = quartz_alloc(Q, sizeof(quartz_Node *));
		if(node->children == NULL) return QUARTZ_ENOMEM;
		node->childCap = 1;
	}
	if(node->childCount == node->childCap) {
		size_t newCap = node->childCap * 2;
		quartz_Node **newBuf = quartz_realloc(Q, node->children, sizeof(quartz_Node *) * node->childCap, sizeof(quartz_Node *) * newCap);
		if(newBuf == NULL) return QUARTZ_ENOMEM;
		node->children = newBuf;
		node->childCap = newCap;
	}
	node->children[node->childCount++] = child;
	return QUARTZ_OK;
}

static quartz_Node *quartzI_popChild(quartz_Node *node) {
	return node->children[--node->childCount];
}

quartz_Errno quartzI_dumpAST(quartz_Buffer *buf, quartz_Node *node) {
	quartz_Errno err = QUARTZ_OK;
	size_t initial = buf->len;
	
	err = quartz_bufputc(buf, '(');
	if(err) goto fail;
	err = quartz_bufputn(buf, node->type);
	if(err) goto fail;
	err = quartz_bufputc(buf, ' ');
	if(err) goto fail;
	err = quartz_bufputls(buf, node->str, node->strlen);
	if(err) goto fail;
	if(node->strlen > 0) {
		err = quartz_bufputc(buf, ' ');
		if(err) goto fail;
	}
	err = quartz_bufputn(buf, node->line);
	if(err) goto fail;
	for(size_t i = 0; i < node->childCount; i++) {
		err = quartz_bufputc(buf, ' ');
		if(err) goto fail;
		err = quartzI_dumpAST(buf, node->children[i]);
		if(err) goto fail;
	}
	err = quartz_bufputc(buf, ')');
	if(err) goto fail;

fail:
	if(err) buf->len = initial;
	return err;
}

void quartzP_initParser(quartz_Thread *Q, quartz_Parser *p, const char *s) {
	p->Q = Q;
	p->s = s;
	p->off = 0;
	p->curline = 1;
	p->curToken.tt = QUARTZ_TOK_EOF;
	p->curToken.len = 0;
	p->errloc = 0;
	p->pErr = QUARTZ_PARSE_OK;
	p->allNodes = NULL;
	return;
}

void quartzP_freeParser(quartz_Parser *p) {
	quartz_Node *n = p->allNodes;
	while(n) {
		quartz_Node *cur = n;
		n = n->next;
		quartzP_freeAST(p->Q, cur);
	}
}

static bool quartzP_mustSkipToken(quartz_Token *t) {
	return t->tt == QUARTZ_TOK_WHITESPACE || t->tt == QUARTZ_TOK_COMMENT;
}

static quartz_Errno quartzP_consume(quartz_Parser *p) {
	do {
		if(p->curToken.tt != QUARTZ_TOK_EOF) {
			p->curline += quartzI_countLines(p->curToken.s, p->curToken.len);
		}
		p->off += p->curToken.len;
		quartz_TokenError e = quartzI_lexAt(p->s, p->off, &p->curToken);
		if(e) {
			p->pErr = QUARTZ_PARSE_ELEX;
			p->lexErr = e;
			p->errloc = p->curline;
			return QUARTZ_ESYNTAX;
		}
	} while(quartzP_mustSkipToken(&p->curToken));
	return QUARTZ_OK;
}

quartz_Errno quartzP_peekToken(quartz_Parser *p, quartz_Token *t) {
	if(p->curToken.tt == QUARTZ_TOK_EOF) {
		quartz_Errno err = quartzP_consume(p);
		if(err) return err;
	}
	*t = p->curToken;
	return QUARTZ_OK;
}

quartz_Errno quartzP_nextToken(quartz_Parser *p, quartz_Token *t) {
	quartz_Errno err = quartzP_peekToken(p, t);
	if(err) return err;
	p->curToken.tt = QUARTZ_TOK_EOF;
	return QUARTZ_OK;
}

quartz_Errno quartzP_parseExpressionBase(quartz_Parser *p, quartz_Node *parent) {
	size_t l = p->curline;
	quartz_Token t;
	quartz_Errno err;

	err = quartzP_nextToken(p, &t);
	if(err) return err;

	if(t.tt == QUARTZ_TOK_NUM) {
		quartz_Node *node = quartzI_allocAST(p, QUARTZ_NODE_NUM, l, t.s, t.len);
		if(node == NULL) return QUARTZ_ENOMEM;
		return quartzI_addNodeChild(p->Q, parent, node);
	}

	if(t.tt == QUARTZ_TOK_STR) {
		quartz_Node *node = quartzI_allocAST(p, QUARTZ_NODE_STR, l, t.s, t.len);
		if(node == NULL) return QUARTZ_ENOMEM;
		return quartzI_addNodeChild(p->Q, parent, node);
	}

	if(t.tt == QUARTZ_TOK_KEYWORD) {
		const char *arr[] = {"null", "true", "false"};
		size_t arrlen = sizeof(arr) / sizeof(arr[0]);
		for(size_t i = 0; i < arrlen; i++) {
			if(quartzI_strleqlc(t.s, t.len, arr[i])) {
				quartz_Node *node = quartzI_allocAST(p, QUARTZ_NODE_KEYWORDCONST, l, t.s, t.len);
				if(node == NULL) return QUARTZ_ENOMEM;
				return quartzI_addNodeChild(p->Q, parent, node);
			}
		}
	}

	if(t.tt == QUARTZ_TOK_IDENT) {
		quartz_Node *node = quartzI_allocAST(p, QUARTZ_NODE_VARIABLE, l, t.s, t.len);
		if(node == NULL) return QUARTZ_ENOMEM;

		while(1) {
			l = p->curline;
			err = quartzP_peekToken(p, &t);
			if(err) return err;

			if(t.s[0] == '.') {
				err = quartzP_nextToken(p, &t);
				if(err) return err;
				
				err = quartzP_nextToken(p, &t);
				if(err) return err;

				if(t.tt != QUARTZ_TOK_IDENT) {
					p->pErr = QUARTZ_PARSE_EBADTOK;
					p->tokExpected = "<identifier>";
					p->errloc = l;
					return QUARTZ_ESYNTAX;
				}

				quartz_Node *field = quartzI_allocAST(p, QUARTZ_NODE_FIELD, l, t.s, t.len);
				if(field == NULL) {
					return QUARTZ_ENOMEM;
				}
				err = quartzI_addNodeChild(p->Q, field, node);
				if(err) return err;
				node = field;
				continue;
			} else if (t.s[0] == '[') {
				err = quartzP_nextToken(p, &t);
				if(err) {
					return err;
				}

				quartz_Node *index = quartzI_allocAST(p, QUARTZ_NODE_INDEX, l, t.s, t.len);
				err = quartzI_addNodeChild(p->Q, index, node);
				if(err) return err;
				
				err = quartzP_parseExpression(p, index);
				if(err) {
					return err;
				}
				
				node = index;
				
				err = quartzP_nextToken(p, &t);
				if(err) {
					return err;
				}

				if(t.s[0] != ']') {
					p->pErr = QUARTZ_PARSE_EBADTOK;
					p->tokExpected = "]";
					p->errloc = p->curline;
					return QUARTZ_ESYNTAX;
				}
				continue;
			} else if (t.s[0] == '(') {
				err = quartzP_nextToken(p, &t);
				if(err) {
					return err;
				}

				quartz_Node *call = quartzI_allocAST(p, QUARTZ_NODE_CALL, l, "", 0);
				if(call == NULL) {
					return QUARTZ_ENOMEM;
				}
				err = quartzI_addNodeChild(p->Q, call, node);
				if(err) return err;

				while(1) {
					l = p->curline;
					err = quartzP_peekToken(p, &t);
					if(err) {
						return err;
					}
					if(t.s[0] == ')') {
						err = quartzP_nextToken(p, &t);
						if(err) {
							return err;
						}
						break;
					}

					err = quartzP_parseExpression(p, call);
					if(err) {
						return err;
					}

					err = quartzP_nextToken(p, &t);
					if(err) {
						return err;
					}

					if(t.s[0] == ')') break;
					if(t.s[0] == ',') continue;
					p->pErr = QUARTZ_PARSE_EBADTOK;
					p->tokExpected = ",";
					p->errloc = p->curline;
					return QUARTZ_ESYNTAX;
				}

				node = call;
				continue;
			}
			break;
		}

	ident_done:
		return quartzI_addNodeChild(p->Q, parent, node);
	}
	
	if(t.s[0] == '[') {
		quartz_Node *n = quartzI_allocAST(p, QUARTZ_NODE_LIST, p->curline, "", 0);
		if(n == NULL) return quartz_oom(p->Q);

		while(1) {
			l = p->curline;
			err = quartzP_peekToken(p, &t);
			if(err) {
				return err;
			}
			if(t.s[0] == ']') {
				err = quartzP_nextToken(p, &t);
				if(err) {
					return err;
				}
				break;
			}

			err = quartzP_parseExpression(p, n);
			if(err) {
				return err;
			}

			err = quartzP_nextToken(p, &t);
			if(err) {
				return err;
			}

			if(t.s[0] == ']') break;
			if(t.s[0] == ',') continue;
			p->pErr = QUARTZ_PARSE_EBADTOK;
			p->tokExpected = ",";
			return QUARTZ_ESYNTAX;
		}

		return quartzI_addNodeChild(p->Q, parent, n);
	}
	
	if(t.s[0] == '(') {
		err = quartzP_parseExpression(p, parent);
		if(err) return err;

		err = quartzP_nextToken(p, &t);
		if(err) return err;

		if(t.s[0] != ')') {
			p->pErr = QUARTZ_PARSE_EBADTOK;
			p->tokExpected = ")";
			p->errloc = p->curline;
			return QUARTZ_ESYNTAX;
		}
		return QUARTZ_OK;
	}

	p->pErr = QUARTZ_PARSE_EEXPR;
	p->errloc = l;
	return QUARTZ_ESYNTAX;
}

static bool quartzP_binOpBp(const char *s, size_t len, size_t *left, size_t *right) {
	const char *comp[] = {"==", "!=", ">=", "<=", ">", "<"};
	size_t complen = sizeof(comp) / sizeof(comp[0]);
	for(size_t i = 0; i < complen; i++) {
		if(quartzI_strleqlc(s, len, comp[i])) {
			*left = 101;
			*right = 102;
			return true;
		}
	}
	if(s[0] == '^' || s[0] == '&' || s[0] == '|') {
		*left = 201;
		*right = 202;
		return true;
	}
	if(s[0] == '+' || s[0] == '-') {
		*left = 203;
		*right = 204;
		return true;
	}
	if(s[0] == '*' || s[0] == '/' || s[0] == '%') {
		*left = 205;
		*right = 206;
		return true;
	}
	if(len == 2 && s[0] == '.' && s[1] == '.') {
		*left = 301;
		*right = 302;
		return true;
	}
	return false;
}

quartz_Errno quartzP_parseExpressionPratt(quartz_Parser *p, quartz_Node *parent, size_t minBp) {
	quartz_Errno err;

	err = quartzP_parseExpressionBase(p, parent);
	if(err) return err;

	while(1) {
		size_t l = p->curline;
		quartz_Token t;
		err = quartzP_peekToken(p, &t);
		if(err) return err;

		if(t.tt == QUARTZ_TOK_EOF) break;

		size_t left, right;
		if(!quartzP_binOpBp(t.s, t.len, &left, &right)) break;

		err = quartzP_nextToken(p, &t);
		if(err) return err;

		err = quartzP_parseExpressionPratt(p, parent, right);
		if(err) return err;

		quartz_Node *opNode = quartzI_allocAST(p, QUARTZ_NODE_OP, l, t.s, t.len);
		if(opNode == NULL) return QUARTZ_ENOMEM;
	
		quartz_Node *rightNode = quartzI_popChild(parent);
		quartz_Node *leftNode = quartzI_popChild(parent);

		err = quartzI_addNodeChild(p->Q, opNode, leftNode);
		if(err) {
			return err;
		}
		err = quartzI_addNodeChild(p->Q, opNode, rightNode);
		if(err) return err;

		err = quartzI_addNodeChild(p->Q, parent, opNode);
		if(err) return err;
	}
	return QUARTZ_OK;
}

quartz_Errno quartzP_parseExpression(quartz_Parser *p, quartz_Node *parent) {
	return quartzP_parseExpressionPratt(p, parent, 0);
}

quartz_Errno quartzP_parseStatement(quartz_Parser *p, quartz_Node *parent, quartz_ParserBlockFlags flags) {
	bool topscope = (flags & QUARTZ_PBLOCK_TOPSCOPE) != 0;
	bool loop = (flags & QUARTZ_PBLOCK_LOOP) != 0;
	size_t l = p->curline;
	quartz_Token t;
	quartz_Errno err;

	err = quartzP_peekToken(p, &t);
	if(err) return err;

	if(quartzI_strleqlc(t.s, t.len, "local")) {
		err = quartzP_nextToken(p, &t);
		if(err) return err;

		quartz_Token name;
		err = quartzP_nextToken(p, &name);
		if(err) return err;

		if(name.tt != QUARTZ_TOK_IDENT) {
			p->pErr = QUARTZ_PARSE_EBADTOK;
			p->tokExpected = "<identifier>";
			p->errloc = p->curline;
			return QUARTZ_ESYNTAX;
		}

		quartz_Node *node = quartzI_allocAST(p, QUARTZ_NODE_LOCAL, l, name.s, name.len);
		if(node == NULL) return QUARTZ_ENOMEM;

		err = quartzI_addNodeChild(p->Q, parent, node);
		if(err) return err;

		err = quartzP_peekToken(p, &t);
		if(err) return err;

		if(t.s[0] == '=') {
			err = quartzP_nextToken(p, &t);
			if(err) return err;
			return quartzP_parseExpression(p, node);
		}
		return QUARTZ_OK;
	}

	if(t.tt == QUARTZ_TOK_IDENT || t.s[0] == '(') {
		err = quartzP_parseExpression(p, parent);
		if(err) return err;
		quartz_Node *var = parent->children[parent->childCount-1];
		if(var->type == QUARTZ_NODE_CALL) return QUARTZ_OK;

		if(var->type == QUARTZ_NODE_VARIABLE || var->type == QUARTZ_NODE_FIELD || var->type == QUARTZ_NODE_INDEX) {
			parent->childCount--;
			err = quartzP_nextToken(p, &t);
			if(err) return err;
			if(t.s[0] != '=') {
				p->pErr = QUARTZ_PARSE_EBADTOK;
				p->tokExpected = "=";
				p->errloc = p->curline;
				return QUARTZ_ESYNTAX;
			}
			quartz_Node *node = quartzI_allocAST(p, QUARTZ_NODE_ASSIGN, p->curline, "", 0);
			if(node == NULL) return QUARTZ_ENOMEM;
			err = quartzI_addNodeChild(p->Q, node, var);
			if(err) return err;
			err = quartzP_parseExpression(p, node);
			if(err) return err;
			return quartzI_addNodeChild(p->Q, parent, node);
		}

		p->pErr = QUARTZ_PARSE_ESTMT;
		p->errloc = l;
		return QUARTZ_ESYNTAX;
	}

	if(quartzI_strleqlc(t.s, t.len, "if")) {
		err = quartzP_nextToken(p, &t);
		if(err) return err;

		// TODO: evaluate if ifs should always be if(cond) {} and never if cond {}
		// smth smth parsing structs vs blocks

		// this is an abomination against our lord and savior
		quartz_Node *node = quartzI_allocAST(p, QUARTZ_NODE_IF, p->curline, "", 0);
		if(node == NULL) return QUARTZ_ENOMEM;

		err = quartzP_parseExpression(p, node);
		if(err) return err;

		err = quartzP_nextToken(p, &t);
		if(err) return err;
		
		if(t.s[0] != '{') {
			p->pErr = QUARTZ_PARSE_EBADTOK;
			p->tokExpected = "{";
			p->errloc = p->curline;
			return QUARTZ_ESYNTAX;
		}

		quartz_Node *block = quartzI_allocAST(p, QUARTZ_NODE_BLOCK, p->curline, "", 0);
		err = quartzI_addNodeChild(p->Q, node, block);
		if(err) return err;

		err = quartzP_parseStatementBlock(p, block, 0);
		if(err) return err;

		return quartzI_addNodeChild(p->Q, parent, node);
	}
	
	if(quartzI_strleqlc(t.s, t.len, "while")) {
		err = quartzP_nextToken(p, &t);
		if(err) return err;

		// TODO: evaluate if ifs should always be if(cond) {} and never if cond {}
		// smth smth parsing structs vs blocks

		// this is an abomination against our lord and savior
		quartz_Node *node = quartzI_allocAST(p, QUARTZ_NODE_WHILE, p->curline, "", 0);
		if(node == NULL) return QUARTZ_ENOMEM;

		err = quartzP_parseExpression(p, node);
		if(err) return err;

		err = quartzP_nextToken(p, &t);
		if(err) return err;
		
		if(t.s[0] != '{') {
			p->pErr = QUARTZ_PARSE_EBADTOK;
			p->tokExpected = "{";
			p->errloc = p->curline;
			return QUARTZ_ESYNTAX;
		}

		quartz_Node *block = quartzI_allocAST(p, QUARTZ_NODE_BLOCK, p->curline, "", 0);
		err = quartzI_addNodeChild(p->Q, node, block);
		if(err) return err;

		err = quartzP_parseStatementBlock(p, block, 0);
		if(err) return err;

		return quartzI_addNodeChild(p->Q, parent, node);
	}

	p->pErr = QUARTZ_PARSE_ESTMT;
	p->errloc = l;
	return QUARTZ_ESYNTAX;
}

quartz_Errno quartzP_parseStatementBlock(quartz_Parser *p, quartz_Node *parent, quartz_ParserBlockFlags flags) {
	bool topscope = (flags & QUARTZ_PBLOCK_TOPSCOPE) != 0;
	bool loop = (flags & QUARTZ_PBLOCK_LOOP) != 0;
	while(true) {
		quartz_Token t;
		quartz_Errno err;

		err = quartzP_peekToken(p, &t);
		if(err) return err;

		if(t.tt == QUARTZ_TOK_EOF) {
			if(topscope) goto over;
			goto badOver;
		} else if(quartzI_strleqlc(t.s, t.len, "}")) {
			if(topscope) goto badOver;
			goto over;
		} else if(t.s[0] == ';') {
			err = quartzP_nextToken(p, &t);
			if(err) return err;
		} else {
			err = quartzP_parseStatement(p, parent, flags);
			if(err) return err;
		}
		continue;
	badOver:
		p->pErr = QUARTZ_PARSE_EBADTOK;
		p->tokExpected = (flags & QUARTZ_PBLOCK_TOPSCOPE) ? "<EOF>" : "}";
		p->errloc = p->curline;
		return QUARTZ_ESYNTAX;
	over:
		err = quartzP_consume(p);
		if(err) return err;
		return QUARTZ_OK;
	}
}

quartz_Errno quartzP_parse(quartz_Thread *Q, quartz_Parser *p, quartz_Node *program) {
	quartz_Errno e = quartzP_parseStatementBlock(p, program, QUARTZ_PBLOCK_TOPSCOPE);
	if(e != QUARTZ_OK && e != QUARTZ_ESYNTAX) {
		p->pErr = QUARTZ_PARSE_EINT;
		p->intErrno = e;
	}
	return e;
}
