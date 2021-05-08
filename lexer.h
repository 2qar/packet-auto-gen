#include <stddef.h>
#include <stdint.h>

struct token {
	char *start;
	char sep;
	size_t len;
	struct token *next;
};

struct token *lexer_parse(char *buf);
