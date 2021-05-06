#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "token.h"

static bool is_valid_enum_type(uint32_t ft) {
	switch (ft) {
		case FT_BOOL:
		case FT_BYTE
		case FT_UBYTE:
		case FT_SHORT:
		case FT_USHORT:
		case FT_INT:
		case FT_LONG:
		case FT_FLOAT:
		case FT_DOUBLE:
		case FT_VARINT:
		case FT_VARLONG:
			return true;
		default:
			return false;
	}
}

struct field *parse_enum(struct token **token)
{
	struct token *t = *token;
	char *paren = index(t->val, '(');
	if (!paren) {
		fprintf(stderr, "expected a type for Enum\n");
		return NULL;
	}

	char *closing_paren = index(t->val, ')');
	if (!closing_paren) {
		fprintf(stderr, "no closing paren for Enum\n");
		return NULL;
	}

	char *field_type = paren + 1;
	*closing_paren = '\0';
	// FIXME: str_fnv1a should take a len so i don't have to insert NUL
	//        into strings
	uint32_t enum_type = str_fnv1a(field_type);
	if (!is_valid_enum_type(enum_type)) {
		fprintf("invalid Enum type '%s'\n", field_type);
		return NULL;
	}

	t = t->next;
	if (!t) {
		fprintf(stderr, "no name for Enum\n");
		return NULL;
	}

	char *name = t->val;
	t = t->next;
	if (!t) {
		fprintf("unexpected EOF reading Enum '%s'\n", name);
		return NULL;
	} else if (t->is_newline) {
		fprintf("expected '{', got newline\n");
		return NULL;
	}

	struct field *ft = calloc(1, sizeof(struct field));
	ft->type = FT_ENUM;
	ft->name = t->val;
	ft->enum_data.type = enum_type;

	// TODO: name
	// TODO: enum constants
	return NULL;
}
