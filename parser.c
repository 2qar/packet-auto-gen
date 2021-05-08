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
	if (t->sep && t->sep == '\n') {
		fprintf(f, "\\n");
	} else if (t->sep) {
		fputc(t->sep, f);
	} else {
		for (size_t i = 0; i < t->len; ++i)
			fputc(t->start[i], f);
	}
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
	if (arg_start->sep != '(') {
		fprintf(stderr, "unexpected token '%c' when parsing '", arg_start->sep);
		put_token(stderr, type);
		fprintf(stderr, "' args\n");
		return NULL;
	}
	// FIXME: support multiple arguments + types w/ args as arguments
	struct token *arg = arg_start->next;
	if (!arg->start) {
		fprintf(stderr, "expected identifier, got '%c'\n", arg->sep);
		return NULL;
	}

	bool args_invalid = false;
	uint32_t arg_type = str_fnv1a(arg->start, arg->len);
	switch (field->type) {
		case FT_ENUM:
			if (!type_valid_for_enum(arg_type)) {
				fprintf(stderr, "invalid enum type '");
				put_token(stderr, arg);
				fprintf(stderr, "'\n");
				args_invalid = true;
			} else {
				field->enum_data.type = arg_type;
			}
			break;
		case FT_ARRAY:
			if (!type_valid_for_array(arg_type)) {
				fprintf(stderr, "invalid array type '");
				put_token(stderr, arg);
				fprintf(stderr, "'\n");
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
				fprintf(stderr, "invalid string length '");
				put_token(stderr, arg);
				fprintf(stderr, "'\n");
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
		fprintf(stderr, "unknown field type '");
		put_token(stderr, type);
		fprintf(stderr, "'\n");
		return NULL;
	}

	bool has_args = field_type_has_args(field_hash);
	bool has_paren = token_equals(type->next, "(");
	if (has_args && !has_paren) {
		fprintf(stderr, "expected args for type '\n");
		put_token(stderr, type);
		fprintf(stderr, "'\n");
		return NULL;
	} else if (!has_args && has_paren) {
		fprintf(stderr, "unexpected args for type '");
		put_token(stderr, type);
		fprintf(stderr, "'\n");
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
		fprintf(stderr, "expected a name, got a '");
		put_token(stderr, name_tok);
		fprintf(stderr, "'\n");
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
		fprintf(stderr, "expected 'if' at start of conditional, got '");
		put_token(stderr, cond_start);
		fprintf(stderr, "'\n");
		return NULL;
	}
	cond_start = cond_start->next;

	struct token *cond_end = cond_start;
	while (cond_end && (cond_end->start || valid_conditional_seperator(cond_end->sep))) {
		cond_end = cond_end->next;
	}
	if (cond_end->sep != ')') {
		fprintf(stderr, "unexpected '");
		put_token(stderr, cond_end);
		fprintf(stderr, "' in conditional\n");
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
	while (c && c->start && token_equals(c->next, "\n")) {
		c = c->next->next;
		++constants_len;
	}
	if (!c || !token_equals(c, "}")) {
		fprintf(stderr, "bad constant '");
		put_token(stderr, c);
		fprintf(stderr, "'\n");
		return NULL;
	}

	char **constants = calloc(constants_len, sizeof(char *));
	size_t i = 0;
	while (!token_equals(c, "}")) {
		size_t constant_len = c->len + 1;
		char *constant = calloc(constant_len, sizeof(char));
		snprintf(constant, constant_len, "%s", c->start);
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
