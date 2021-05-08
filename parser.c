#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "lexer.h"
#include "parser.h"

static void put_token(FILE *f, struct token *t)
{
	for (size_t i = 0; i < t->len; ++i)
		fputc(t->start[i], f);
}

static void perr(const char *s1, struct token *t, const char *s2)
{
	fprintf(stderr, "%zu:%zu | %s", t->line, t->col, s1);
	put_token(stderr, t);
	fprintf(stderr, "%s", s2);
}

char *next_nonblank(char *s)
{
	while (*s != '\0' && (*s == ' ' || *s == '\t' || *s == '\n'))
		++s;

	if (*s == '\0')
		return NULL;
	else
		return s;
}

static bool type_valid_for_enum(uint32_t ft)
{
	// TODO
	return ft;
}

static bool type_valid_for_array(uint32_t ft)
{
	// TODO
	return ft;
}

static bool field_type_is_valid(uint32_t ft)
{
	// TODO
	return ft;
}

static bool field_type_has_args(uint32_t ft)
{
	return ft == FT_ENUM ||
		ft == FT_ARRAY ||
		ft == FT_UNION ||
		ft == FT_STRING;
}

static struct token *read_type_args(struct token *type, struct field *field)
{
	struct token *arg_start = type->next;
	if (!token_equals(arg_start, "(")) {
		fprintf(stderr, "unexpected token '%c' when parsing '", arg_start->start[0]);
		put_token(stderr, type);
		fprintf(stderr, "' args\n");
		return NULL;
	}
	// FIXME: support multiple arguments + types w/ args as arguments
	struct token *arg = arg_start->next;
	if (arg->is_sep) {
		perr("expected identifier, got '", arg, "'\n");
		return NULL;
	}

	bool args_invalid = false;
	uint32_t arg_type = str_fnv1a(arg->start, arg->len);
	switch (field->type) {
		case FT_ENUM:
			if (!type_valid_for_enum(arg_type)) {
				perr("invalid enum type '", arg, "'\n");
				args_invalid = true;
			} else {
				field->enum_data.type = arg_type;
			}
			break;
		case FT_ARRAY:
			if (!type_valid_for_array(arg_type)) {
				perr("invalid array type '", arg, "'\n");
				args_invalid = true;
			} else {
				field->array.type = arg_type;
			}
			break;
		case FT_UNION:;
			// TODO: check that arg is an enum
			size_t name_len = arg->len + 1;
			char *name = calloc(name_len, sizeof(char));
			snprintf(name, name_len, "%s", arg->start);
			field->union_data.enum_field_name = name;
			break;
		case FT_STRING:
			if (sscanf(arg->start, "%zu", &field->string_len) != 1) {
				perr("invalid string length '", arg, "'\n");
				args_invalid = true;
			}
			break;
	}

	if (args_invalid) {
		return NULL;
	} else {
		return arg->next->next;
	}
}

struct token *read_field_type(struct token *type, struct field *field)
{
	uint32_t field_hash = str_fnv1a(type->start, type->len);
	if (!field_type_is_valid(field_hash)) {
		perr("unknown field type '", type, "'\n");
		return NULL;
	}

	bool has_args = field_type_has_args(field_hash);
	bool has_paren = token_equals(type->next, "(");
	if (has_args && !has_paren) {
		perr("expected args for type '", type, "'\n");
		return NULL;
	} else if (!has_args && has_paren) {
		perr("unexpected args for type '", type, "'\n");
		return NULL;
	}

	field->type = field_hash;
	if (has_args) {
		return read_type_args(type, field);
	} else {
		return type->next;
	}
}

struct token *read_field_name(struct token *name_tok, struct field *field)
{
	if (!name_tok->start) {
		perr("expected a name, got a '", name_tok, "'\n");
		return NULL;
	}

	size_t name_len = name_tok->len + 1;
	char *name = calloc(name_len, sizeof(char));
	snprintf(name, name_len, "%s", name_tok->start);
	field->name = name;
	return name_tok->next;
}

static bool valid_conditional_seperator(char c)
{
	return c == '<' ||
		c == '>' ||
		c == '!' ||
		c == '=' ||
		c == '&' ||
		c == '|';
}

struct token *read_conditional(struct token *paren, struct field *field)
{
	struct token *cond_start = paren->next;
	if (!token_equals(cond_start, "if")) {
		perr("expected 'if' at start of conditional, got '", cond_start, "'\n");
		return NULL;
	}
	cond_start = cond_start->next;

	struct token *cond_end = cond_start;
	while (cond_end && (cond_end->start || valid_conditional_seperator(cond_end->start[0]))) {
		cond_end = cond_end->next;
	}
	if (!token_equals(cond_end, ")")) {
		perr("unexpected '", cond_end, "' in conditional\n");
		return NULL;
	}

	// FIXME: hacky, should be using tokens
	char *closing_paren = index(cond_start->start, ')');

	struct field *old_field = calloc(1, sizeof(struct field));
	memcpy(old_field, field, sizeof(struct field));
	field->type = FT_OPTIONAL;
	size_t condition_len = closing_paren - cond_start->start + 1;
	field->optional.condition = calloc(condition_len, sizeof(char));
	snprintf(field->optional.condition, condition_len, "%s", cond_start->start);
	field->optional.field = old_field;

	return cond_end->next;
}

/*
static bool is_valid_constant_name(char *line)
{
	while (*line != '\0' && *line != '\n' && (isalpha(*line) || *line == '_')) {
		++line;
	}
	return *line == '\n';
}
*/

struct token *parse_enum(struct token *first_constant, struct field *field)
{
	size_t constants_len = 0;
	struct token *c = first_constant;
	while (c && !c->is_sep && token_equals(c->next, "\n")) {
		c = c->next->next;
		++constants_len;
	}
	if (!c || !token_equals(c, "}")) {
		perr("bad constant '", c, "'\n");
		return NULL;
	}

	c = first_constant;
	char **constants = calloc(constants_len, sizeof(char *));
	size_t i = 0;
	while (!token_equals(c, "}")) {
		size_t constant_len = c->len + 1;
		char *constant = calloc(constant_len, sizeof(char));
		snprintf(constant, constant_len, "%s", c->start);
		printf("constant: %s\n", constant);
		constants[i] = constant;
		c = c->next->next;
		++i;
	}

	field->enum_data.constants_len = constants_len;
	field->enum_data.constants = constants;
	return c->next;
}

struct field *parse_fields(char **start)
{
	/* TODO */
	printf("%s\n", *start);
	return NULL;
}
