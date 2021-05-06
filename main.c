#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "hash.h"

size_t file_bytes(const char *filename, char **out)
{
	FILE *f = fopen(filename, "r");
	if (!f) {
		perror("fopen");
		return 0;
	}
		
	fseek(f, 0L, SEEK_END);
	long f_len = ftell(f);
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

struct token {
	bool is_newline;
	char *val;
	struct token *next;
};

char *next_nonblank(char *s)
{
	while (*s != '\0' && (*s == ' ' || *s == '\t' || *s == '\n'))
		++s;

	if (*s == '\0')
		return NULL;
	else
		return s;
}

void put_indent(int indent)
{
	while (indent) {
		printf("    ");
		--indent;
	}
}

struct token *tokenize(char *bytes) 
{
	struct token *t = calloc(1, sizeof(struct token));
	struct token *head = t;
	char *idx = next_nonblank(bytes);
	if (!idx) {
		free(t);
		return NULL;
	}

	while (idx) {
		char *blank = strpbrk(idx, " \n");
		if (blank - idx > 0) {
			size_t name_len = blank - idx + 1;
			// TODO: make the tokens actual tokens, don't duplicate
			//       the string, just store an index + size in tokens
			//       and use that along with the byte array to get
			//       the string
			t->val = calloc(name_len, sizeof(char));
			snprintf(t->val, name_len, "%s", idx);
		}
		if (*blank == '\n') {
			t->next = calloc(1, sizeof(struct token));
			t = t->next;
			t->is_newline = true;
		}

		if (blank && (idx = next_nonblank(blank))) {
			t->next = calloc(1, sizeof(struct token));
			t = t->next;
		}
	}
	return head;
}

void free_tokens(struct token *head)
{
	while (head) {
		struct token *t = head;
		head = head->next;
		free(t->val);
		free(t);
	}
}

void pretty_print(struct token *head)
{
	struct token *t = head;
	int indent_level = 0;
	while (t != NULL) {
		if (!t->is_newline) {
			printf("%s ", t->val);
			if (!strncmp(t->val, "{", 1)) {
				++indent_level;
			}
		} else {
			if (t->next != NULL && !t->next->is_newline && !strcmp(t->next->val, "}"))
				--indent_level;
			putchar('\n');
			put_indent(indent_level);
		}
		t = t->next;
	}
}

void put_id(const char *packet_filename, int id)
{
	printf("#define PROTOCOL_ID_");
	char c = *packet_filename;
	while (c != '\0' && c != '.') {
		if (isalpha(c))
			c = toupper(c);
		putchar(c);

		++packet_filename;
		c = *packet_filename;
	}
	printf(" 0x%x\n", id);
}

bool token_is(struct token *t, char *s)
{
	return t && !strcmp(t->val, s);
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
	if (!bytes)
		return 1;

	char *name = packet_name(packet_filename);
	printf("packet name: %s\n", name);

	struct token *head = tokenize(bytes);
	if (head == NULL) {
		fprintf(stderr, "empty file\n");
		return 1;
	}

	int id;
	if (!token_is(head, "id") || 
			!token_is(head->next, "=") || 
			!head->next->next ||
			!head->next->next->val ||
			0 == sscanf(head->next->next->val, "0x%x", &id)) {
		fprintf(stderr, "malformed ID\n");
		return 1;
	}
	put_id(packet_filename, id);

	struct token *t = head;
	while (t && !t->is_newline) {
		t = t->next;
	}
	if (!t || !t->next) {
		fprintf(stderr, "no fields!\n");
		return 1;
	}
	t = t->next;

	bool err = false;
	while (t != NULL && !err) {
		printf("%s\n", t->val);
		char *paren = index(t->val, '(');
		size_t name_len;
		if (!paren) {
			name_len = strlen(t->val);
		} else {
			name_len = paren - t->val;
			t->val[name_len] = '\0';
		}
		printf("'%s'\n", t->val);
		uint32_t hash = str_fnv1a(t->val);
		if (paren)
			t->val[name_len] = '(';

		struct field *f;
		switch (hash) {
			case FT_ENUM:
				field = parse_enum(&t);
				break;
			default:
				break;
		}

		while (t->next && !t->next->is_newline) {
			t = t->next;
		}
		if (t->next) {
			t = t->next->next;
		} else {
			t = t->next;
		}
	}

	free(bytes);
	free_tokens(head);
	return 0;
}
