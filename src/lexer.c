#include "quartz.h"
#include "utils.h"
#include "lexer.h"

const char *quartzI_lexErrors[QUARTZ_TOK_ECOUNT] = {
	[QUARTZ_TOK_OK] = "ok",
	[QUARTZ_TOK_EBADNUM] = "malformed number",
	[QUARTZ_TOK_EBADCHAR] = "bad character",
	[QUARTZ_TOK_EBADESC] = "bad escape",
	[QUARTZ_TOK_EUNFINISHEDSTR] = "unfinished string",
};

bool quartzI_isKeyword(const char *s, size_t len) {
	if(quartzI_strleqlc(s, len, "local")) return true;
	if(quartzI_strleqlc(s, len, "global")) return true;
	if(quartzI_strleqlc(s, len, "fun")) return true;
	if(quartzI_strleqlc(s, len, "return")) return true;
	if(quartzI_strleqlc(s, len, "if")) return true;
	if(quartzI_strleqlc(s, len, "else")) return true;
	if(quartzI_strleqlc(s, len, "do")) return true;
	if(quartzI_strleqlc(s, len, "while")) return true;
	if(quartzI_strleqlc(s, len, "for")) return true;
	if(quartzI_strleqlc(s, len, "in")) return true;
	if(quartzI_strleqlc(s, len, "and")) return true;
	if(quartzI_strleqlc(s, len, "or")) return true;
	if(quartzI_strleqlc(s, len, "not")) return true;
	if(quartzI_strleqlc(s, len, "break")) return true;
	if(quartzI_strleqlc(s, len, "continue")) return true;
	if(quartzI_strleqlc(s, len, "null")) return true;
	if(quartzI_strleqlc(s, len, "true")) return true;
	if(quartzI_strleqlc(s, len, "false")) return true;
	return false;
}

// NULL-terminated symbol list
const char *quartzI_lexSymbols[] = {
	// Sorted by length in descending order
	// as an optimization in the lexer algorithm

	// 3-char
	"<<<",
	">>>",
	"...",
	// 2-char
	"==",
	"!=",
	"<=",
	">=",
	"<<",
	">>",
	"#(", // begin tuple
	"#[", // begin set
	"#{", // begin map
	"**", // exponentiation
	"..", // concatination
	"//", // integer division
	// 1-char
	"+", // addition
	"-", // subtraction
	"*", // multiplication, or dereferencing with .
	"/", // division
	"%", // remainder
	"~", // bitwise not
	"^", // bitwise xor
	"&", // bitwise and, or take reference
	"|", // bitwise or
	"=", // assign
	">", // greater than
	"<", // less than
	",",
	".",
	":",
	";",
	"(",
	")",
	"[",
	"]",
	"{",
	"}",
	NULL,
};

static size_t quartzI_escapeLen(const char *s) {
	if(s[0] == '\\') {
		if(s[1] == 'n') return 1; // new-line
		if(s[1] == 't') return 1; // tab
		if(s[1] == 'r') return 1; // carriage return
		if(s[1] == 'f') return 1; // next page
		if(s[1] == 'v') return 1; // vertical tab
		if(s[1] == 'a') return 1; // bell
		if(s[1] == 'b') return 1; // backspace
		if(s[1] == 'e') return 1; // escape
		if(s[1] == 'x') {
			// hex byte escape
			if(!quartzI_ishex(s[2])) return 0;
			if(!quartzI_ishex(s[3])) return 0;
			return 4;
		}
		return 0;
	}
	return 1;
}

quartz_TokenError quartzI_lexAt(const char *fullS, size_t off, quartz_Token *t) {
	const char *s = fullS + off;
	char c = *s;
	t->s = s;
	if(c == '\0') {
		t->len = 0;
		t->tt = QUARTZ_TOK_EOF;
		return QUARTZ_TOK_OK;
	}
	if(quartzI_iswhitespace(c)) {
		size_t len = 1;
		while(quartzI_iswhitespace(s[len])) len++;
		t->len = len;
		t->tt = QUARTZ_TOK_WHITESPACE;
		return QUARTZ_TOK_OK;
	}
	for(size_t i = 0; quartzI_lexSymbols[i] != NULL; i++) {
		const char *sym = quartzI_lexSymbols[i];
		size_t symlen = quartzI_strlen(sym);
		if(quartzI_strleql(sym, symlen, s, symlen)) {
			t->len = symlen;
			t->tt = QUARTZ_TOK_SYMBOL;
			return QUARTZ_TOK_OK;
		}
	}
	if(c == '#') {
		size_t len = 2;
		while(true) {
			if(s[len] == '\0') break;
			if(s[len] == '\n') {
				len++;
				break;
			}
			len++;
		}
		t->len = len;
		t->tt = QUARTZ_TOK_COMMENT;
		return QUARTZ_TOK_OK;
	}
	if(quartzI_isnum(c)) {
		size_t len = 2;
		int base = 10;
		if(s[1] == 'x')
			base = 16;
		else if(s[1] == 'o')
			base = 8;
		else if(s[1] == 'b')
			base = 2;
		else
			len = 1;

		while(quartzI_isbase(s[len], base)) len++;
		if(s[len] == '.') {
			len++;
			while(quartzI_isbase(s[len], base)) len++;
		}
		if(s[len] == 'e') {
			len++;
			if(s[len] == '+' || s[len] == '-') len++;
			while(quartzI_isbase(s[len], base)) len++;
		}
		// imaginary numbers
		if(s[len] == 'i') len++;
		if(quartzI_isalpha(s[len])) return QUARTZ_TOK_EBADNUM;
		t->len = len;
		t->tt = QUARTZ_TOK_NUM;
		return QUARTZ_TOK_OK;
	}
	if(c == '"' || c == '\'') {
		size_t len = 1;
		while(true) {
			if(s[len] == c) {
				len++;
				break;
			}
			if(s[len] == '\0') return QUARTZ_TOK_EUNFINISHEDSTR;
			size_t escLen = quartzI_escapeLen(s + len);
			if(escLen == 0) return QUARTZ_TOK_EBADESC;
			len += escLen;
		}
		t->len = len;
		t->tt = QUARTZ_TOK_STR;
		return QUARTZ_TOK_OK;
	}
	if(quartzI_isident(c)) {
		size_t len = 1;
		while(quartzI_isident(s[len])) len++;
		t->len = len;
		t->tt = quartzI_isKeyword(s, len) ? QUARTZ_TOK_KEYWORD : QUARTZ_TOK_IDENT;
		return QUARTZ_TOK_OK;
	}
	return QUARTZ_TOK_EBADCHAR;
}

size_t quartzI_countLines(const char *s, size_t off) {
	size_t l = 0;
	// C is beautiful
	for(size_t i = 0; i <= off; i++) if(s[i] == '\n') l++;
	return l;
}
