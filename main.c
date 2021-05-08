#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lexer.h"
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

void print_tokens(struct token *t)
{
	while (t != NULL) {
		printf("%zd:%zd ", t->line, t->col);
		if (token_equals(t, "\n")) {
			printf("'\\n'\n");
		} else {
			putchar('\'');
			for (size_t i = 0; i < t->len; ++i)
				putchar(t->start[i]);
			printf("'\n");
		}
		t = t->next;
	}
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

	struct token *tokens = lexer_parse(bytes);
	print_tokens(tokens);

	struct token *t = tokens->next->next->next;
	while (token_equals(t, "\n")) {
		t = t->next;
	}
	struct field *head = calloc(1, sizeof(struct field));
	struct field *f = head;
	while (t) {
		t = read_field_type(t, f);
		if (!t)
			break;
		t = read_field_name(t, f);
		if (!t)
			break;
		if (token_equals(t, "("))
			t = read_conditional(t, f);
		if (!t)
			break;

		// TODO move to a read_field_data() or smth
		if (token_equals(t, "{")) {
			switch (f->type) {
				case FT_ENUM:
					t = parse_enum(t->next->next, f);
					break;
				default:
					break;
			}
		}

		if (token_equals(t, "\n")) {
			t = t->next;
			struct field *next_field = calloc(1, sizeof(struct field));
			f->next = next_field;
			f = next_field;
		}
	}

	free(bytes);
	return 0;
}
