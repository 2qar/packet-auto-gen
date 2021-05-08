#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool token_equals(struct token *t, const char *s)
{
	if (!t)
		return false;

	if (t->sep) {
		return t->sep == s[0];
	} else if (t->start) {
		return !strncmp(t->start, s, t->len);
	} else {
		return false;
	}
}

struct token *lexer_parse(char *buf)
{
	struct token *t = calloc(1, sizeof(struct token));
	struct token *tokens = t;
	size_t token_len = 0;
	size_t i = 0;

	while (buf[i] != '\0') {
		if (!t->start && isalnum(buf[i])) {
			t->start = &buf[i];
			token_len = 0;
		}

		switch (buf[i]) {
			case '=':
			case '(':
			case ')':
			case '{':
			case '}':
			case ',':
			case '\n':
				if (t->start && token_len > 0) {
					t->len = token_len;
					t->next = calloc(1, sizeof(struct token));
					t = t->next;
				} else {
					t->start = NULL;
				}
				t->sep = buf[i];
				t->next = calloc(1, sizeof(struct token));
				t = t->next;
				break;
			case ' ':
				if (t->start && token_len > 0) {
					t->len = token_len;
					t->next = calloc(1, sizeof(struct token));
					t = t->next;
				}
				break;
		}

		if (t->start)
			++token_len;
		++i;
	}

	return tokens;
}
