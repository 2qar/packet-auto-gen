#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "parser.h"

void put_id(const char *packet_filename, int id)
{
	printf("#define PROTOCOL_ID_");
	char c = *packet_filename;
	while (c != '\0') {
			c = toupper(c);
		putchar(c);

		++packet_filename;
		c = *packet_filename;
	}
	printf(" 0x%x\n", id);
}

void put_includes()
{
	puts("#include <stdbool.h>");
	puts("#include <string.h>");
	puts("#include <stdint.h>");
	puts("#include <stdio.h>");
	printf("#include \"%s/conn.h\"\n", CHOWDER_DIR);
	printf("#include \"%s/packet.h\"\n", CHOWDER_DIR);
}

static char *ftype_to_ctype(struct field *f)
{
	switch (f->type) {
		case FT_ENUM:
			if (f->enum_data.type_field->type == FT_STRING)
				return "char*";
			else
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
		case FT_IDENTIFIER:
		case FT_CHAT:
			return "char*";
		case FT_UUID:
			return "uint64_t";
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
	struct enum_constant *c = f->enum_data.constants;
	while (c != NULL) {
		put_indent(indent + 1);
		put_enum_constant(packet_name, f->name, c->name);
		printf(",\n");
		c = c->next;
	}
	put_indent(indent);
	printf("}");
}

static void put_fields(char *packet_name, struct field *, size_t indent);

static void put_struct(char *packet_name, struct field *f, size_t indent)
{
	printf(" {\n");
	put_fields(packet_name, f->struct_fields, indent + 1);
	put_indent(indent);
	printf("}");
}

static void put_fields(char *name, struct field *f, size_t indent)
{
	while (f->type) {
		if (f->type != FT_EMPTY) {
			put_indent(indent);
			printf("%s", ftype_to_ctype(f));
			switch (f->type) {
				case FT_ENUM:
					if (f->enum_data.type_field->type != FT_STRING)
						put_enum(name, f, indent);
					break;
				case FT_STRUCT:
					put_struct(name, f, indent);
					break;
				case FT_STRUCT_ARRAY:
					printf(" %s_%s *", name, f->struct_array.struct_name);
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
			// FIXME: kinda hacky, maybe have a field_name_prefix() and
			//        field_name_suffix() function for printing stuff
			//        before and after the name?
			if (f->type == FT_UUID)
				printf("[2]");
			printf(";\n");
		}

		f = f->next;
	}
}

static void generate_struct(char *name, struct field *fields)
{
	printf("struct %s {\n", name);
	put_fields(name, fields, 1);
	printf("};\n");
}

static void generate_struct_array_structs(char *name, struct field *f)
{
	while (f->type != 0) {
		switch (f->type) {
			case FT_STRUCT:
				generate_struct_array_structs(name, f->struct_fields);
				break;
			case FT_UNION:
				generate_struct_array_structs(name, f->union_data.fields);
				break;
			case FT_STRUCT_ARRAY:
				generate_struct_array_structs(name, f->struct_array.fields);
				break;
		}

		if (f->type == FT_STRUCT_ARRAY) {
			printf("struct %s_%s {\n", name, f->struct_array.struct_name);
			put_fields(name, f->struct_array.fields, 1);
			printf("};\n");
		}

		f = f->next;
	}
}

void generate_structs(char *name, struct field *fields)
{
	generate_struct_array_structs(name, fields);
	generate_struct(name, fields);
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
		case FT_IDENTIFIER:
			return "packet_write_string";
		case FT_UUID:
			return "packet_write_bytes";
		default:
			return NULL;
	}
}

static void put_path_iter(struct field *f)
{
	if (f == NULL) {
		printf("pack->");
	} else {
		put_path_iter(f->parent);

		if (f->name != NULL) {
			printf("%s", f->name);
			if (f->type == FT_STRUCT_ARRAY)
				printf("[i_%s]", f->name);
			putchar('.');
		}
	}
}

static void put_path(struct field *f)
{
	put_path_iter(f->parent);
}

static void write_field(char *packet_name, struct field *, size_t indent);
static void write_fields(char *packet_name, struct field *, size_t indent);

static void write_string_enum_check(struct field *enum_field, size_t indent)
{
	put_indent(indent);
	printf("static const char *%s_values[] = { ", enum_field->name);
	struct enum_constant *c = enum_field->enum_data.constants;
	while (c != NULL) {
		printf("\"%s\", ", c->name);
		c = c->next;
	}
	printf("NULL };\n");

	put_indent(indent);
	printf("size_t i_%s = 0;\n", enum_field->name);
	put_indent(indent);
	printf("while (%s_values[i_%s] != NULL && strcmp(%s_values[i_%s], ", enum_field->name, enum_field->name, enum_field->name, enum_field->name);
	put_path(enum_field);
	printf("%s))\n", enum_field->name);
	put_indent(indent + 1);
	printf("++i_%s;\n", enum_field->name);

	put_indent(indent);
	printf("if (%s_values[i_%s] == NULL) {\n", enum_field->name, enum_field->name);
	put_indent(indent + 1);
	printf("fprintf(stderr, \"'%%s' isn't a valid value for %s\", ", enum_field->name);
	put_path(enum_field);
	printf("%s);\n", enum_field->name);
	put_indent(indent + 1);
	printf("return -1;\n");
	put_indent(indent);
	printf("}\n");
}

static void write_struct_array(char *packet_name, struct field *f, size_t indent)
{
	put_indent(indent);
	printf("for (size_t i_%s = 0; i_%s < ", f->name, f->name);
	put_path(f);
	printf("%s_len; ++i_%s) {\n", f->name, f->name);
	write_fields(packet_name, f->struct_array.fields, indent + 1);
	put_indent(indent);
	printf("}\n");
}

static void write_union(char *packet_name, struct field *union_field, size_t indent)
{
	put_indent(indent);
	struct field *enum_field = union_field->union_data.enum_field;
	printf("switch (");
	put_path(enum_field);
	printf("%s) {\n", enum_field->name);
	struct field *f = union_field->union_data.fields;
	struct enum_constant *c = enum_field->enum_data.constants;
	// FIXME: check during parsing or some other stage that
	//        the enum's constants_len == union_data.fields len,
	//        and that the values are 0 -> len-1
	while (f->type != 0 && c != NULL) {
		put_indent(indent + 1);
		printf("case ");
		// FIXME: re-writing the constants again with the packet name here
		//        feels wrong, the enum constants should be re-written
		//        with the packet name as a part of them during parsing
		//        or some other stage so the packet_name doesn't need to
		//        be passed around everywhere just for this function
		put_enum_constant(packet_name, enum_field->name, c->name);
		printf(":;\n");
		write_field(packet_name, f, indent + 2);
		put_indent(indent + 2);
		printf("break;\n");
		f = f->next;
		c = c->next;
	}

	// for each enum constant,
	//   if the corresponding field isn't an empty,
	//     write it out
	put_indent(indent);
	printf("}\n");
}

static void write_string(struct field *f, size_t indent)
{
	put_indent(indent);
	printf("size_t %s_len = strlen(", f->name);
	put_path(f);
	printf("%s);\n", f->name);
	put_indent(indent);
	printf("if (%s_len > %zu) {\n", f->name, f->string_max_len);
	put_indent(indent + 1);
	printf("fprintf(stderr, \"'");
	put_path(f);
	printf("%s' exceeded max len (%zu)\\n\");\n", f->name, f->string_max_len);
	put_indent(indent + 1);
	printf("return -1;\n");
	put_indent(indent);
	printf("}\n");

	put_indent(indent);
	printf("packet_write_string(p, %s_len, ", f->name);
	put_path(f);
	printf("%s);\n", f->name);
}

static void write_uuid(struct field *f, size_t indent)
{
	put_indent(indent);
	printf("packet_write_bytes(p, 16, ");
	put_path(f);
	printf("%s);\n", f->name);
}

static void put_string(size_t len, const char *s)
{
	for (size_t i = 0; i < len; ++i)
		putchar(s[i]);
}

static void put_operand(struct condition *condition, size_t i)
{
	if (condition->operands[i].is_field) {
		put_path(condition->operands[i].field);
		printf("%s", condition->operands[i].field->name);
	} else
		put_string(condition->operands[i].string_len, condition->operands[i].string);
}

static void put_condition(struct condition *condition)
{
	put_operand(condition, 0);
	if (condition->op) {
		printf(" %s ", condition->op);
		put_operand(condition, 1);
	}

}

static void write_field(char *packet_name, struct field *f, size_t indent)
{
	char *func_name = NULL;
	if (f->condition != NULL) {
		put_indent(indent);
		printf("if (");
		put_condition(f->condition);
		printf(") {\n");
		++indent;
	}
	switch (f->type) {
		case FT_ENUM:
			if (f->enum_data.type_field->type == FT_STRING)
				write_string_enum_check(f, indent);
			func_name = write_function_name(f->enum_data.type_field->type);
			break;
		case FT_STRUCT:;
			write_fields(packet_name, f->struct_fields, indent);
			break;
		case FT_STRUCT_ARRAY:
			write_struct_array(packet_name, f, indent);
			break;
		case FT_UNION:
			write_union(packet_name, f, indent);
			break;
		case FT_STRING:
		case FT_IDENTIFIER:
		case FT_CHAT:
			write_string(f, indent);
			break;
		case FT_UUID:
			write_uuid(f, indent);
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
		put_path(f);
		printf("%s);\n", f->name);
	}
	if (f->condition != NULL) {
		--indent;
		put_indent(indent);
		printf("}\n");
	}
}

static void write_fields(char *packet_name, struct field *f, size_t indent)
{
	while (f->type) {
		write_field(packet_name, f, indent);
		f = f->next;
	}
}

void generate_write_function(int id, char *name, struct field *f)
{
	printf("int protocol_write_%s(struct conn *c, struct %s *pack) {\n", name, name);
	printf("\tstruct packet *p = c->packet;\n");
	printf("\tmake_packet(p, 0x%x);\n", id);
	write_fields(name, f, 1);
	printf("\treturn conn_write_packet(c);\n");
	printf("}\n");
}
