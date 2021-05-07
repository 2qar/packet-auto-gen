#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"

size_t file_bytes(const char *filename, char **out)
{
	FILE *f = fopen(filename, "r");
	if (!f) {
		perror("fopen");
		return 0;
	}
		
	fseek(f, 0L, SEEK_END);
	size_t f_len = ftell(f);
	rewind(f);
	char *f_bytes = calloc(f_len + 1, sizeof(char));
	size_t n_read = fread(f_bytes, 1, f_len, f);
	fclose(f);
	if (n_read != f_len) {
		free(f_bytes);
		fprintf(stderr, "read %zd, expected %ld\n", n_read, f_len);
		return 0;
	}
	*out = f_bytes;
	return n_read;
}

char *packet_name(const char *packet_filename)
{
	char *name = NULL;
	char *dot = index(packet_filename, '.');
	size_t name_len;
	if (dot) {
		name_len = dot - packet_filename + 1;
	} else {
		name_len = strlen(packet_filename) + 1;
	}
	name = calloc(name_len, sizeof(char));
	snprintf(name, name_len, "%s", packet_filename);
	return name;
}

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

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: pc PACKET_FILE\n");
		return 1;
	}
	char *packet_filename = argv[1];

	char *bytes = NULL;
	size_t bytes_len = file_bytes(packet_filename, &bytes);
	if (!bytes || !bytes_len)
		return 1;

	char *name = packet_name(packet_filename);
	printf("packet name: %s\n", name);

	unsigned id;
	if (!sscanf(bytes, "id = 0x%x\n", &id)) {
		fprintf(stderr, "malformed ID\n");
		return 1;
	}
	put_id(name, id);

	char *next_newline = index(bytes, '\n');
	if (!next_newline) {
		fprintf(stderr, "no file, just an ID\n");
		return 1;
	}

	struct field *field = calloc(1, sizeof(struct field));
	struct field *head = field;
	char *line_start = next_nonblank(next_newline);
	while (line_start) {
		struct field *f = read_field_type(&line_start);
		if (!f)
			break;
		line_start = read_field_name(line_start, f);
		if (!line_start)
			break;
		char *next_symbol = strpbrk(line_start, "({\n");
		if (!next_symbol || *next_symbol == '\0') {
			fprintf(stderr, "expected '(', '{', or '\\n', got EOF\n");
			break;
		} else if (*next_symbol == '(') {
			next_symbol = read_conditional(next_symbol, field);
			if (!next_symbol)
				break;
		}

		if (*next_symbol == '{') {
			next_symbol = next_nonblank(next_symbol + 1);
			switch (f->type) {
				case FT_ENUM:
					next_symbol = parse_enum(next_symbol, f);
					break;
				case FT_UNION:
					f->union_data.fields = parse_fields(&next_symbol);
					break;
				case FT_STRUCT:
				case FT_STRUCT_ARRAY:
					f->fields = parse_fields(&next_symbol);
					break;
			}
			if (!next_symbol)
				break;
		} else if (*next_symbol != '\n') {
			fprintf(stderr, "unexpected symbol '%c' while moving to next field\n", *next_symbol);
			break;
		}
		line_start = next_nonblank(next_symbol);

		field->next = f;
		field = f;
	}

	// at this point, head should point to a complete field tree.
	// with that, a C struct + read/write code shouldn't be too hard
	// to generate.

	free(head);
	free(bytes);
	return 0;
}
