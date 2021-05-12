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

// :(
static bool field_type_is_valid(uint32_t ft)
{
	switch (ft) {
		case FT_BOOL:
		case FT_BYTE:
		case FT_UBYTE:
		case FT_SHORT:
		case FT_USHORT:
		case FT_INT:
		case FT_LONG:
		case FT_FLOAT:
		case FT_DOUBLE:
		case FT_STRING:
		case FT_CHAT:
		case FT_IDENTIFIER:
		case FT_VARINT:
		case FT_VARLONG:
		case FT_ENTITY_METADATA:
		case FT_SLOT:
		case FT_NBT_TAG:
		case FT_POSITION:
		case FT_ANGLE:
		case FT_UUID:
		case FT_ARRAY:
		case FT_ENUM:
		case FT_UNION:
		case FT_STRUCT:
		case FT_EMPTY:
			return true;
		default:
			return false;
	}
	return false;
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
				if (arg_type == FT_STRUCT)
					field->type = FT_STRUCT_ARRAY;
				field->array.type = arg_type;
			}
			break;
		case FT_UNION:;
			// TODO: check that arg is an enum
			size_t name_len = arg->len + 1;
			char *name = calloc(name_len, sizeof(char));
			snprintf(name, name_len, "%s", arg->start);
			struct field *enum_field = calloc(1, sizeof(struct field));
			enum_field->name = name;
			field->union_data.enum_field = enum_field;
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
	if (name_tok->is_sep && field->type == FT_UNION)
		return name_tok;
	else if (name_tok->is_sep) {
		perr("expected a name, got '", name_tok, "'\n");
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
	while (cond_end && (!cond_end->is_sep || valid_conditional_seperator(cond_end->start[0]))) {
		cond_end = cond_end->next;
	}
	if (!token_equals(cond_end, ")")) {
		perr("unexpected '", cond_end, "' in conditional\n");
		return NULL;
	}

	// FIXME: hacky, should be using tokens
	char *closing_paren = strchr(cond_start->start, ')');

	size_t condition_len = closing_paren - cond_start->start + 1;
	field->condition = calloc(condition_len, sizeof(char));
	snprintf(field->condition, condition_len, "%s", cond_start->start);

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
		perr("bad constant '", c, "'");
		if (c && c->next) {
			fprintf(stderr, "; expected '\\n', got '");
			put_token(stderr, c->next);
			fputc('\'', stderr);
		}
		fputc('\n', stderr);
		return NULL;
	}

	c = first_constant;
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

/* used for structs, struct arrays, unions */
static struct token *parse_fields(struct token *first_field, struct field *fields)
{
	struct token *t = first_field;
	struct field *f = fields;
	while (t && !token_equals(t, "}")) {
		t = parse_field(t, f);
		if (t)
			f = f->next;
	}
	if (t)
		return t->next;
	else
		return NULL;
}

static bool type_has_body(uint32_t ft)
{
	return ft == FT_ENUM ||
		ft == FT_STRUCT ||
		ft == FT_UNION ||
		ft == FT_STRUCT_ARRAY;
}

struct token *read_field_body(struct token *open_brace, struct field *f)
{
	struct token *t;
	switch (f->type) {
		case FT_ENUM:
			t = parse_enum(open_brace->next->next, f);
			break;
		case FT_UNION:
		case FT_STRUCT:
		case FT_STRUCT_ARRAY:;
			struct field *fields = calloc(1, sizeof(struct field));
			t = parse_fields(open_brace->next->next, fields);
			if (f->type == FT_UNION)
				f->union_data.fields = fields;
			else
				f->fields = fields;
			break;
		default:
			break;
	}
	return t;
}

struct token *parse_field(struct token *t, struct field *f)
{
	t = read_field_type(t, f);
	if (!t)
		return NULL;
	t = read_field_name(t, f);
	if (!t)
		return NULL;
	if (token_equals(t, "("))
		t = read_conditional(t, f);
	if (!t)
		return NULL;

	bool has_body = token_equals(t, "{");
	bool should_have_body = type_has_body(f->type);
	if (has_body && should_have_body) {
		t = read_field_body(t, f);
	} else if (!has_body && should_have_body) {
		fprintf(stderr, "%zu:%zu | type should have a body, but it doesn't\n",
				t->line, t->col);
		return NULL;
	} else if (has_body && !should_have_body) {
		fprintf(stderr, "%zu:%zu | type 0x%x shouldn't have a body, but it seems to have one\n",
				t->line, t->col, f->type);
		return NULL;
	}

	if (!t)
		return NULL;

	if (token_equals(t, "\n")) {
		t = t->next;
		struct field *next_field = calloc(1, sizeof(struct field));
		f->next = next_field;
	}
	return t;
}

// FIXME: all the stuff below seems like it belongs in a seperate file

static struct field *find_field(struct field *f, uint32_t f_type, char *f_name)
{
	struct field *res = NULL;
	while (!res && f->type != 0) {
		if (f->type == f_type && !strcmp(f->name, f_name))
			res = f;
		else if (f->type == FT_STRUCT || f->type == FT_STRUCT_ARRAY)
			res = find_field(f->fields, f_type, f_name);
		else if (f->type == FT_UNION)
			res = find_field(f->union_data.fields, f_type, f_name);

		f = f->next;
	}
	return res;
}

static bool resolve_union_enums_iter(struct field *root, struct field *f)
{
	bool err = false;
	while (!err && f->type != 0) {
		switch (f->type) {
			case FT_STRUCT:
			case FT_STRUCT_ARRAY:
				err = resolve_union_enums_iter(root, f->fields);
				break;
			case FT_UNION:;
				struct field *partial = f->union_data.enum_field;
				char *name = partial->name;
				free(partial);
				struct field *enum_field = find_field(root, FT_ENUM, name);
				if (enum_field == NULL) {
					fprintf(stderr, "union '%s' depends on non-existent enum '%s'\n", f->name, name);
					err = true;
				}
				free(name);
				f->union_data.enum_field = enum_field;
				break;
			default:
				break;
		}
		f = f->next;
	}
	return err;
}

bool resolve_union_enums(struct field *root)
{
	return resolve_union_enums_iter(root, root);
}
