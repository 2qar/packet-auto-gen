#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lexer.h"
#include "parser.h"
#include "gen.h"

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
	char *slash = strrchr(packet_filename, '/');
	if (slash)
		packet_filename = slash + 1;

	char *name = NULL;
	char *dot = strchr(packet_filename, '.');
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
	unsigned id;
	if (!sscanf(bytes, "id = 0x%x\n", &id)) {
		fprintf(stderr, "malformed ID\n");
		return 1;
	}
	put_id(name, id);

	struct token *tokens = lexer_parse(bytes);

	struct token *t = tokens;
	while (!token_equals(t, "\n")) {
		t = t->next;
	}
	while (token_equals(t, "\n")) {
		t = t->next;
	}
	struct field *head = calloc(1, sizeof(struct field));
	struct field *f = head;
	while (t && t->line != 0) {
		t = parse_field(t, f);
		if (f->next)
			f = f->next;
	}
	if (!t)
		return 1;
	free_tokens(tokens);

	create_parent_links(head);
	if (resolve_field_name_refs(head))
		return 1;

	put_includes();
	generate_structs(name, head);
	generate_write_function(id, name, head);

	free_fields(head);
	free(bytes);
	free(name);
	return 0;
}
