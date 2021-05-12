#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "parser.h"

void put_includes()
{
	puts("#include <stdbool.h>");
	puts("#include <string.h>");
	puts("#include <stdint.h>");
	puts("#include <stdio.h>");
}

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
		case FT_STRUCT:
		case FT_STRUCT_ARRAY:
			return "struct";
		case FT_STRING:
		case FT_UUID:
		case FT_IDENTIFIER:
		case FT_CHAT:
			return "char*";
		case FT_UNION:
			return "union";
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

static void put_enum(char *packet_name, struct field *f, size_t indent)
{
	printf(" {\n");
	for (size_t i = 0; i < f->enum_data.constants_len; ++i) {
		put_indent(indent + 1);
		put_enum_constant(packet_name, f->name, f->enum_data.constants[i]);
		printf(",\n");
	}
	put_indent(indent);
	printf("}");
}

static void put_fields(char *packet_name, struct field *, size_t indent);

static void put_struct(char *packet_name, struct field *f, size_t indent)
{
	printf(" {\n");
	put_fields(packet_name, f->fields, indent + 1);
	put_indent(indent);
	printf("}");
}

static void put_fields(char *name, struct field *f, size_t indent)
{
	while (f->type) {
		if (f->type != FT_EMPTY) {
			put_indent(indent);
			printf("%s", ftype_to_ctype(f->type));
			switch (f->type) {
				case FT_ENUM:
					put_enum(name, f, indent);
					break;
				case FT_STRUCT:
					put_struct(name, f, indent);
					break;
				case FT_STRUCT_ARRAY:
					put_struct(name, f, indent);
					putchar('*');
					break;
				case FT_ARRAY:
					printf("/* TODO: array */ ");
					break;
				case FT_UNION:
					printf(" {\n");
					put_fields(name, f->union_data.fields, indent + 1);
					put_indent(indent);
					printf("}");
				default:
					break;
			}

			if (f->name)
				printf(" %s", f->name);
			printf(";\n");
		}

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
	bool is_array;
	struct field_path *next;
};

static void put_path(struct field_path *f)
{
	printf("%s->", f->field_name);
	f = f->next;
	while (f != NULL) {
		printf("%s", f->field_name);
		if (f->is_array)
			printf("[i_%s]", f->field_name);
		putchar('.');
		f = f->next;
	}
}

static struct field_path *end_of_path(struct field_path *f)
{
	while (f->next != NULL) {
		f = f->next;
	}
	return f;
}

static void write_field(char *packet_name, struct field *, struct field_path *, size_t indent);
static void write_fields(char *packet_name, struct field *, struct field_path *, size_t indent);

static void write_struct_array(char *packet_name, struct field *f, struct field_path *path, size_t indent)
{
	put_indent(indent);
	printf("for (size_t i_%s = 0; i_%s < ", f->name, f->name);
	put_path(path);
	printf("%s_len; ++i_%s) {\n", f->name, f->name);
	struct field_path path_next = { .field_name = f->name, .is_array = true, .next = NULL };
	struct field_path *path_end = end_of_path(path);
	path_end->next = &path_next;
	write_fields(packet_name, f->fields, path, indent + 1);
	path_end->next = NULL;
	put_indent(indent);
	printf("}\n");
}

static void write_union(char *packet_name, struct field *union_field, struct field_path *path, size_t indent)
{
	put_indent(indent);
	struct field *enum_field = union_field->union_data.enum_field;
	printf("switch (%s->%s) {\n", path->field_name, enum_field->name);
	struct field *f = union_field->union_data.fields;
	size_t i = 0;
	// FIXME: check during parsing or some other stage that
	//        the enum's constants_len == union_data.fields len,
	//        and that the values are 0 -> len-1
	while (f->type != 0 && i < enum_field->enum_data.constants_len) {
		put_indent(indent + 1);
		printf("case ");
		// FIXME: re-writing the constants again with the packet name here
		//        feels wrong, the enum constants should be re-written
		//        with the packet name as a part of them during parsing
		//        or some other stage so the packet_name doesn't need to
		//        be passed around everywhere just for this function
		put_enum_constant(packet_name, enum_field->name, enum_field->enum_data.constants[i]);
		printf(":;\n");
		write_field(packet_name, f, path, indent + 2);
		put_indent(indent + 2);
		printf("break;\n");
		f = f->next;
		++i;
	}

	// for each enum constant,
	//   if the corresponding field isn't an empty,
	//     write it out
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
	printf("packet_write_string(p, %s_len, ", f->name);
	put_path(path);
	printf("%s);\n", f->name);
}

static void write_field(char *packet_name, struct field *f, struct field_path *path, size_t indent)
{
	char *func_name = NULL;
	switch (f->type) {
		case FT_ENUM:
			func_name = write_function_name(f->enum_data.type);
			break;
		case FT_STRUCT:;
			struct field_path path_next = { .field_name = f->name, .is_array = false, .next = NULL };
			struct field_path *path_end = end_of_path(path);
			path_end->next = &path_next;
			write_fields(packet_name, f->fields, path, indent);
			path_end->next = NULL;
			break;
		case FT_STRUCT_ARRAY:
			write_struct_array(packet_name, f, path, indent);
			break;
		case FT_UNION:
			write_union(packet_name, f, path, indent);
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
}

static void write_fields(char *packet_name, struct field *f, struct field_path *path, size_t indent)
{
	while (f->type) {
		write_field(packet_name, f, path, indent);
		f = f->next;
	}
}

void generate_write_function(char *name, struct field *f)
{
	printf("int protocol_write_%s(struct conn *c, struct %s *pack) {\n", name, name);
	printf("\tstruct packet *p = c->packet;\n");
	struct field_path path = { .field_name = "pack", .next = NULL };
	write_fields(name, f, &path, 1);
	printf("\treturn conn_write_packet(c);\n");
	printf("}\n");
}
