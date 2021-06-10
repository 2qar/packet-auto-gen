#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "lexer.h"
#include "parser.h"
#include "parser_conf.h"

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

static bool valid_type(uint32_t t, const uint32_t valid[])
{
	size_t i = 0;
	while (valid[i] != 0 && t != valid[i])
		++i;
	return valid[i] != 0;
}

static struct token *read_type_args(struct token *, struct field *);

static struct token *read_enum_args(uint32_t arg_type, struct token *arg, struct field *field)
{
	if (!valid_type(arg_type, valid_enum_types)) {
		perr("invalid enum type '", arg, "'\n");
		return NULL;
	}

	field->enum_data.type_field = calloc(1, sizeof(struct field));
	field->enum_data.type_field->type = arg_type;
	if (valid_type(arg_type, valid_types_with_args)) {
		struct token *closing_paren = read_type_args(arg, field->enum_data.type_field);
		if (closing_paren != NULL)
			return closing_paren->next;
		else
			return closing_paren;
	} else {
		return arg->next->next;
	}
}

static struct token *read_array_args(uint32_t arg_type, struct token *arg, struct field *field)
{
	struct token *struct_name_tok = arg->next;
	if (!valid_type(arg_type, valid_types)) {
		perr("invalid array type '", arg, "'\n");
		return NULL;
	} else if (arg_type == FT_STRUCT && struct_name_tok->is_sep) {
		perr("expected a struct name, got '", arg, "'\n");
		return NULL;
	} else if (arg_type == FT_STRUCT) {
		field->type = FT_STRUCT_ARRAY;
		size_t struct_name_len = arg->next->len + 1;
		char *struct_name = calloc(struct_name_len, sizeof(char));
		snprintf(struct_name, struct_name_len, "%s", arg->next->start);
		field->struct_array.struct_name = struct_name;
		return struct_name_tok->next->next;
	} else {
		// TODO: support types w/ args
		field->array.type = arg_type;
		return arg->next->next;
	}
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

	uint32_t arg_type = str_fnv1a(arg->start, arg->len);
	struct token *arg_end = NULL;
	switch (field->type) {
		case FT_ENUM:
			arg_end = read_enum_args(arg_type, arg, field);
			break;
		case FT_ARRAY:
			arg_end = read_array_args(arg_type, arg, field);
			break;
		case FT_UNION:;
			size_t name_len = arg->len + 1;
			char *name = calloc(name_len, sizeof(char));
			snprintf(name, name_len, "%s", arg->start);
			struct field *enum_field = calloc(1, sizeof(struct field));
			enum_field->name = name;
			field->union_data.enum_field = enum_field;

			arg_end = arg->next->next;
			break;
		case FT_STRING:
			if (sscanf(arg->start, "%zu", &field->string_max_len) != 1) {
				perr("invalid string length '", arg, "'\n");
			} else {
				arg_end = arg->next->next;
			}
			break;
	}
	return arg_end;
}

static struct token *read_field_type(struct token *type, struct field *field)
{
	uint32_t field_hash = str_fnv1a(type->start, type->len);
	if (!valid_type(field_hash, valid_types)) {
		perr("unknown field type '", type, "'\n");
		return NULL;
	}

	bool has_args = valid_type(field_hash, valid_types_with_args);
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
		switch (field->type) {
			case FT_CHAT:
				field->string_max_len = 262144;
				break;
			case FT_IDENTIFIER:
				field->string_max_len = 32767;
				break;
			default:
				break;
		}
		return type->next;
	}
}

static struct token *read_field_name(struct token *name_tok, struct field *field)
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

static bool is_num_literal(size_t len, const char *s)
{
	size_t i = 0;
	if (!strncmp(s, "0x", 2))
		i += 2;

	while (i < len && isdigit(s[i]))
		++i;

	return i == len;
}

static bool is_valid_operator(char c1, char c2)
{
	if (c1 == '<' || c1 == '>')
		return c2 == 0 || c2 == '=';
	else if (c1 == '&')
		return c2 == 0 || c2 == '&';
	else if (c1 == '|')
		return c2 == 0 || c2 == '|';
	else if (c1 == '=' || c1 == '!')
		return c2 == '=';
	else
		return false;
}

static struct token *read_operator(struct token *start, char **operator)
{
	struct token *end = start->next;
	char other_sep = 0;
	if (end->is_sep) {
		other_sep = *end->start;
		end = end->next;
	}

	if (!is_valid_operator(*start->start, other_sep)) {
		fprintf(stderr, "%zu:%zu | invalid operator '%c", start->line, start->col, *start->start);
		if (other_sep != 0)
			fputc(other_sep, stderr);
		fprintf(stderr, "'\n");
		return NULL;
	}

	char *op = calloc(3, sizeof(char));
	snprintf(op, 3, "%c%c", *start->start, other_sep);
	*operator = op;
	return end;
}

static struct token *read_conditional(struct token *paren, struct field *field)
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
	} else if (cond_start->is_sep) {
		perr("expected first operand, got '", cond_start, "'\n");
		return NULL;
	}

	// FIXME: only one or two operands, no complicated logic.
	//        maybe that's for the best?
	struct condition *condition = calloc(1, sizeof(struct condition));
	if (is_num_literal(cond_start->len, cond_start->start))
		condition->operands[0].is_field = false;
	else
		condition->operands[0].is_field = true;
	condition->operands[0].string_len = cond_start->len;
	condition->operands[0].string = cond_start->start;

	cond_start = cond_start->next;
	if (cond_start != cond_end) {
		struct token *next_operand = read_operator(cond_start, &condition->op);
		if (next_operand == NULL) {
			return NULL;
		} else if (next_operand == cond_end) {
			perr("expected operand, got '", next_operand, "'\n");
			return NULL;
		}

		if (is_num_literal(next_operand->len, next_operand->start))
			condition->operands[1].is_field = false;
		else
			condition->operands[1].is_field = true;

		condition->operands[1].string_len = next_operand->len;
		condition->operands[1].string = next_operand->start;
	}

	field->condition = condition;

	return cond_end->next;
}

static struct token *parse_int_constant(struct token *constant_start, struct enum_constant *enum_constant)
{
	struct token *constant_end = NULL;
	if (token_equals(constant_start->next, "=")) {
		struct token *value_tok = constant_start->next->next;
		if (value_tok == NULL ||
				sscanf(value_tok->start, "%d\n", &enum_constant->value) != 1)
			return NULL;
		enum_constant->has_value = true;
		constant_end = value_tok;
	} else if (token_equals(constant_start->next, "\n")) {
		constant_end = constant_start;
	}

	if (constant_end != NULL) {
		size_t constant_len = constant_start->len + 1;
		enum_constant->name = calloc(constant_len, sizeof(char));
		snprintf(enum_constant->name, constant_len, "%s", constant_start->start);
		return constant_end->next->next;
	} else {
		return NULL;
	}
}

static struct token *parse_string_constant(struct token *constant_start, struct enum_constant *enum_constant)
{
	if (constant_start->start[0] != '"' ||
			constant_start->start[constant_start->len - 1] != '"' ||
			!token_equals(constant_start->next, "\n"))
		return NULL;

	size_t constant_len = constant_start->len - 1;
	enum_constant->name = calloc(constant_len, sizeof(char));
	snprintf(enum_constant->name, constant_len, "%s", constant_start->start + 1);
	return constant_start->next->next;
}

static struct token *parse_enum(struct token *first_constant, struct field *field)
{
	struct token *(*parse_constant)(struct token *, struct enum_constant *);
	if (first_constant->start[0] == '"')
		parse_constant = parse_string_constant;
	else
		parse_constant = parse_int_constant;

	struct enum_constant *constants = calloc(1, sizeof(struct enum_constant));
	struct enum_constant *constant = constants;
	struct token *c = parse_constant(first_constant, constants);
	struct token *c_prev = c;
	while (c && c->len != 0 && !token_equals(c, "}")) {
		constant->next = calloc(1, sizeof(struct enum_constant));
		constant = constant->next;
		c_prev = c;
		c = parse_constant(c, constant);
	}

	if (!c) {
		perr("invalid constant '", c_prev, "'\n");
		return NULL;
	} else if (c->len == 0) {
		fprintf(stderr, "hit EOF looking for the end of enum %s\n", field->name);
		return NULL;
	}

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

static struct token *read_field_body(struct token *open_brace, struct field *f)
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
			else if (f->type == FT_STRUCT)
				f->struct_fields = fields;
			else
				f->struct_array.fields = fields;
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

static void create_parent_links_iter(struct field *parent, struct field *f)
{
	while (f->type != 0) {
		f->parent = parent;
		switch (f->type) {
			case FT_STRUCT:
				create_parent_links_iter(f, f->struct_fields);
				break;
			case FT_STRUCT_ARRAY:
				create_parent_links_iter(f, f->struct_array.fields);
				break;
			case FT_UNION:
				create_parent_links_iter(f, f->union_data.fields);
				break;
		}
		f = f->next;
	}
}

void create_parent_links(struct field *root)
{
	create_parent_links_iter(NULL, root);
}

// FIXME: all the stuff below seems like it belongs in a seperate file

static struct field *find_field_with_len(struct field *f, uint32_t f_type, size_t f_name_len, char *f_name)
{
	struct field *res = NULL;
	while (!res && f->type != 0) {
		if (!f->name && f->type == f_type)
			res = f;
		if (f->name && (f->type == f_type || f_type == FT_ANY) && !strncmp(f->name, f_name, f_name_len))
			res = f;
		else if (f->type == FT_STRUCT)
			res = find_field_with_len(f->struct_fields, f_type, f_name_len, f_name);
		else if (f->type == FT_STRUCT_ARRAY)
			res = find_field_with_len(f->struct_array.fields, f_type, f_name_len, f_name);
		else if (f->type == FT_UNION)
			res = find_field_with_len(f->union_data.fields, f_type, f_name_len, f_name);

		f = f->next;
	}
	return res;
}

static struct field *find_field(struct field *f, uint32_t f_type, char *f_name)
{
	return find_field_with_len(f, f_type, strlen(f_name), f_name);
}

static bool resolve_condition_name_refs(struct field *root, struct condition *condition)
{
	size_t i = 0;
	bool err = false;
	while (!err && i < 2) {
		if (condition->operands[i].is_field) {
			struct field *f = find_field_with_len(root, FT_ANY,
					condition->operands[i].string_len,
					condition->operands[i].string);
			if (f == NULL) {
				fprintf(stderr, "couldn't put up with this shit anymore\n");
				return true;
			} else {
				condition->operands[i].field = f;
			}
		}
		++i;
	}
	return err;
}

static bool resolve_field_name_refs_iter(struct field *root, struct field *f)
{
	bool err = false;
	while (!err && f->type != 0) {
		if (f->condition != NULL) {
			err = resolve_condition_name_refs(root, f->condition);
		}

		switch (f->type) {
			case FT_STRUCT:
				err = resolve_field_name_refs_iter(root, f->struct_fields);
				break;
			case FT_STRUCT_ARRAY:
				err = resolve_field_name_refs_iter(root, f->struct_array.fields);
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

				if (!err) {
					err = resolve_field_name_refs_iter(root, f->union_data.fields);
				}
				break;
			default:
				break;
		}
		f = f->next;
	}
	return err;
}

bool resolve_field_name_refs(struct field *root)
{
	return resolve_field_name_refs_iter(root, root);
}

void free_fields(struct field *f)
{
	while (f != NULL) {
		if (f->condition)
			free(f->condition->op);

		switch (f->type) {
			case FT_ENUM:
				free(f->enum_data.type_field);
				struct enum_constant *c = f->enum_data.constants;
				struct enum_constant *next;
				while (c != NULL) {
					next = c->next;
					free(c->name);
					free(c);
					c = next;
				}
				break;
			case FT_UNION:;
				free_fields(f->union_data.fields);
				break;
			case FT_STRUCT_ARRAY:
				free(f->struct_array.struct_name);
				free_fields(f->struct_array.fields);
				break;
			case FT_STRUCT:
				free_fields(f->struct_fields);
				break;
			default:
				break;
		}
		free(f->name);
		free(f->condition);
		struct field *next = f->next;
		free(f);
		f = next;
	}
}
