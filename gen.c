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
		default:
			return NULL;
	}
}

void generate_write_function(char *name, struct field *f)
{
	printf("int protocol_write_%s(struct conn *c, struct %s *pack) {\n", name, name);
	printf("\tstruct packet *p = c->packet;\n");
	while (f->type) {
		char *func_name;
		switch (f->type) {
			case FT_ENUM:
				func_name = write_function_name(f->enum_data.type);
				break;
			default:
				func_name = write_function_name(f->type);
				break;
		}
		printf("\t%s(p, pack->%s);\n", func_name, f->name);
		f = f->next;
	}
	printf("\treturn conn_write_packet(p);\n");
	printf("}\n");
}
