#include <ctype.h>
#include <stdio.h>
#include "hash.h"

#include "parser.h"

int main()
{
	size_t strings_len = 10;
	char *strings[] = {
		"Enum",
		"VarInt",
		"UUID",
		"union",
		"struct",
		"String",
		"Array",
		"bool",
		"Chat",
		"Empty",
	};

	for (size_t i = 0; i < strings_len; ++i) {
		uint32_t hash = str_fnv1a(strings[i]);
		printf("#define PACKET_FT_");
		char *s = strings[i];
		while (*s != '\0') {
			putchar(toupper(*s));
			++s;
		}
		printf(" 0x%x\n", hash);
	}

	printf("FT_BOOL: 0x%x\n", FT_BOOL);
}
