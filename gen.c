#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "parser.h"

static char *ftype_to_ctype(uint32_t ft)
{
	switch (ft) {
		case FT_ENUM:
			return "enum";
		case FT_DOUBLE:
			return "double";
		case FT_BOOL:
			return "bool";
		case FT_VARINT:
			return "int32_t";
		case FT_FLOAT:
			return "float";
		default:
			return NULL;
	}
}

static void put_indent(size_t indent)
{
	for (size_t i = 0; i < indent; ++i)
		putchar('\t');
}

static void put_upper(char *s)
{
	while (*s != '\0') {
		if (isalpha(*s))
			putchar(toupper(*s));
		else
			putchar(*s);
		++s;
	}
}

static void put_enum_constant(char *packet_name, char *enum_name, char *constant)
{
	put_upper(packet_name);
	putchar('_');
	put_upper(enum_name);
	putchar('_');
	put_upper(constant);
}

static void put_fields(char *name, struct field *f, size_t indent)
{
	while (f->type) {
		put_indent(indent);
		printf("%s ", ftype_to_ctype(f->type));
		if (f->type == FT_ENUM) {
			printf(" {\n");
			for (size_t i = 0; i < f->enum_data.constants_len; ++i) {
				put_indent(indent + 1);
				put_enum_constant(name, f->name, f->enum_data.constants[i]);
				printf(",\n");
			}
			put_indent(indent);
			printf("} ");
		}
		printf("%s;\n", f->name);

		f = f->next;
	}
}

void generate_struct(char *name, struct field *fields)
{
	printf("struct %s {\n", name);
	put_fields(name, fields, 1);
	printf("};\n");
}

static char *write_function_name(uint32_t ft)
{
	switch (ft) {
		case FT_ENUM:
			return "packet_write_int";
		case FT_DOUBLE:
			return "packet_write_double";
		case FT_BOOL:
			return "packet_write_byte";
		case FT_VARINT:
			return "packet_write_varint";
		case FT_FLOAT:
			return "packet_write_float";
		case FT_STRING:
		case FT_UUID:
		case FT_IDENTIFIER:
			return "packet_write_string";
		default:
			return NULL;
	}
}

struct field_path {
	char *field_name;
	struct field_path *next;
};

static void add_path(struct field_path *f, struct field_path *next)
{
	while (f->next != NULL) {
		f = f->next;
	}
	f->next = next;
}

static void put_path(struct field_path *f)
{
	printf("%s->", f->field_name);
	f = f->next;
	while (f != NULL) {
		printf("%s.", f->field_name);
		f = f->next;
	}
}

static void write_fields(struct field *, struct field_path *, size_t indent);

static void write_struct_array(struct field *f, struct field_path *path, size_t indent)
{
	put_indent(indent);
	// FIXME: this won't work with nested loops
	printf("for (size_t i = 0; i < ");
	put_path(path);
	printf("%s_len; ++i) {\n", f->name);
	struct field_path path_next = { .field_name = f->name, .next = NULL };
	add_path(path, &path_next);
	write_fields(f->fields, path, indent + 1);
	put_indent(indent);
	printf("}\n");
}

static void write_string(struct field *f, struct field_path *path, size_t indent)
{
	put_indent(indent);
	printf("size_t %s_len = strlen(", f->name);
	put_path(path);
	printf("%s);\n", f->name);
	put_indent(indent);
	printf("if (%s_len > %zu) {\n", f->name, f->string_len);
	put_indent(indent + 1);
	printf("fprintf(stderr, \"%%s exceeded max len (%zu)\\n\", %s_len);\n", f->string_len, f->name);
	put_indent(indent + 1);
	printf("return -1;\n");
	put_indent(indent);
	printf("}\n");

	put_indent(indent);
	printf("packet_write_string(pack, %s_len, %s);\n", f->name, f->name);
}

static void write_fields(struct field *f, struct field_path *path, size_t indent)
{
	while (f->type) {
		char *func_name = NULL;
		switch (f->type) {
			case FT_ENUM:
				func_name = write_function_name(f->enum_data.type);
				break;
			case FT_STRUCT:
				write_fields(f->fields, path, indent);
				break;
			case FT_STRUCT_ARRAY:
				write_struct_array(f, path, indent);
				break;
			case FT_STRING:
			case FT_UUID:
			case FT_IDENTIFIER:
				write_string(f, path, indent);
				break;
			case FT_EMPTY:
				break;
			default:
				func_name = write_function_name(f->type);
				break;
		}
		if (func_name != NULL) {
			put_indent(indent);
			printf("%s(p, ", func_name);
			put_path(path);
			printf("%s);\n", f->name);
		}
		f = f->next;
	}
}

void generate_write_function(char *name, struct field *f)
{
	printf("int protocol_write_%s(struct conn *c, struct %s *pack) {\n", name, name);
	printf("\tstruct packet *p = c->packet;\n");
	struct field_path path = { .field_name = "pack", .next = NULL };
	write_fields(f, &path, 1);
	printf("\treturn conn_write_packet(p);\n");
	printf("}\n");
}
