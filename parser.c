#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "hash.h"

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

static char *read_type_args(char *paren, struct field *field)
{
	char *closing_paren = index(paren, ')');
	if (!closing_paren) {
		fprintf(stderr, "no closing paren\n");
		return NULL;
	} else if (closing_paren == paren + 1) {
		fprintf(stderr, "no arguments\n");
		return NULL;
	}

	// FIXME: assumes only one argument
	*closing_paren = '\0';
	char *arg = paren + 1;
	bool args_invalid = false;
	uint32_t arg_type = str_fnv1a(arg);
	switch (field->type) {
		case FT_ENUM:
			if (!type_valid_for_enum(arg_type)) {
				fprintf(stderr, "invalid enum type '%s'\n", arg);
				args_invalid = true;
			} else {
				field->enum_data.type = arg_type;
			}
			break;
		case FT_ARRAY:
			if (!type_valid_for_array(arg_type)) {
				fprintf(stderr, "invalid array type '%s'\n", arg);
				args_invalid = true;
			} else {
				field->array.type = arg_type;
			}
			break;
		case FT_UNION:
			// TODO: check that arg is an enum
			field->union_data.enum_field_name = strdup(arg);
			break;
		case FT_STRING:
			if (sscanf(arg, "%zu", &field->string_len) != 1) {
				fprintf(stderr, "malformed string length '%s'\n", arg);
				args_invalid = true;
			}
			break;
	}
	*closing_paren = ')';

	if (args_invalid) {
		return NULL;
	} else {
		return closing_paren + 1;
	}
}

struct field *read_field_type(char **line_start)
{
	char *name_end = strpbrk(*line_start, " (\n");
	if (!name_end) {
		// TODO: add line numbers to errors
		fprintf(stderr, "unexpected EOF\n");
		return NULL;
	} else if (*name_end == '\n') {
		fprintf(stderr, "unexpected newline\n");
		return NULL;
	}

	bool paren = *name_end == '(';
	*name_end = '\0';
	// TODO: take a len param in str_fnv1a()
	uint32_t field_hash = str_fnv1a(*line_start);
	if (!field_type_is_valid(field_hash)) {
		fprintf(stderr, "unknown field type '%s'\n", *line_start);
		return NULL;
	}

	bool has_args = field_type_has_args(field_hash);
	if (!paren && has_args) {
		fprintf(stderr, "expected opening paren for type '%s', check whitespace\n", *line_start);
		return NULL;
	} else if (paren && !has_args) {
		fprintf(stderr, "unexpected paren for type '%s'\n", *line_start);
		return NULL;
	} else if (paren)
		*name_end = '(';
	else
		*name_end = ' ';

	struct field *field = calloc(1, sizeof(struct field));
	field->type = field_hash;
	if (has_args && !(*line_start = read_type_args(name_end, field))) {
		free(field);
		return NULL;
	}
	return field;
}

char *read_field_name(char *after_type, struct field *field)
{
	if (!after_type || *after_type == '\0') {
		fprintf(stderr, "expected a name, got EOF\n");
		return NULL;
	} else if (*after_type == '\n') {
		fprintf(stderr, "expected a name, got a newline\n");
		return NULL;
	}

	char *name_start = next_nonblank(after_type);
	if (!isalpha(*name_start)) {
		fprintf(stderr, "expected start of a name, got '%c'\n", *name_start);
		return NULL;
	}
	char *name_end = strpbrk(name_start, " ({\n");
	if (*name_end != ' ') {
		fprintf(stderr, "expected a space after name, got a ");
		if (*name_end == '\n')
			fprintf(stderr, "newline\n");
		else
			fprintf(stderr, "'%c'\n", *name_end);
	}

	size_t name_len = name_end - name_start + 1;
	char *name = calloc(name_len, sizeof(char));
	snprintf(name, name_len, "%s", name_start);
	field->name = name;
	return name_end;
}

char *read_conditional(char *paren, struct field *field)
{
	char *closing_paren = strpbrk(paren, "){\n");
	if (!closing_paren) {
		fprintf(stderr, "expected a closing paren, got EOF\n");
		return NULL;
	} else if (*closing_paren == '{') {
		fprintf(stderr, "expected a closing paren, got '{'\n");
		return NULL;
	} else if (*closing_paren == '\n') {
		fprintf(stderr, "expected a closing paren, got a newline\n");
		return NULL;
	} else if (strncmp(paren + 1, "if", 2)) {
		fprintf(stderr, "expected 'if' at start of conditional, got '");
		char *s = paren + 1;
		while (*s != '\0' && *s != ' ')
			fputc(*s, stderr);
		fprintf(stderr, "'\n");
		return NULL;
	}

	struct field *old_field = calloc(1, sizeof(struct field));
	memcpy(old_field, field, sizeof(struct field));
	field->type = FT_OPTIONAL;
	char *condition = next_nonblank(paren + 3);
	size_t condition_len = closing_paren - condition;
	field->optional.condition = calloc(condition_len, sizeof(char));
	snprintf(field->optional.condition, condition_len, "%s", condition);
	field->optional.field = old_field;

	return strpbrk(closing_paren, "{\n");
}

static bool is_valid_constant_name(char *line)
{
	while (*line != '\0' && *line != '\n' && (isalpha(*line) || *line == '_')) {
		++line;
	}
	return *line == '\n';
}

char *parse_enum(char *first_constant, struct field *field)
{
	size_t constants_len = 0;
	char *line = first_constant;
	char *newline = index(line, '\n');
	while (line && *line != '}' && newline && is_valid_constant_name(line)) {
		++constants_len;
		line = next_nonblank(newline);
		newline = index(line, '\n');
	}
	if (!line || !newline) {
		fprintf(stderr, "hit EOF while parsing enum constants\n");
		return NULL;
	} else if (*line != '}') {
		*newline = '\0';
		fprintf(stderr, "invalid constant name '%s'\n", line);
		*newline = '\n';
		return NULL;
	}


	char **constants = calloc(constants_len, sizeof(char *));
	size_t i = 0;
	line = first_constant;
	newline = index(line, '\n');
	while (*line != '}') {
		size_t constant_len = newline - line + 1;
		char *constant = calloc(constant_len, sizeof(char));
		snprintf(constant, constant_len, "%s", line);
		constants[i] = constant;
		line = next_nonblank(newline);
		newline = index(line, '\n');
		++i;
	}

	field->enum_data.constants_len = constants_len;
	field->enum_data.constants = constants;
	return line + 1;
}

struct field *parse_fields(char **start)
{
	/* TODO */
	printf("%s\n", *start);
	return NULL;
}
