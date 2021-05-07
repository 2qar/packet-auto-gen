#include <ctype.h>
#include <stdio.h>
#include "hash.h"

int main()
{
	size_t strings_len = 15;
	char *strings[] = {
		"Byte",
		"UByte",
		"Short",
		"UShort",
		"Int",
		"Long",
		"Float",
		"Double",
		"Identifier",
		"VarLong",
		"EntityMeta",
		"Slot",
		"NBTTag",
		"Position",
		"Angle",
	};

	for (size_t i = 0; i < strings_len; ++i) {
		uint32_t hash = str_fnv1a(strings[i]);
		printf("#define FT_");
		char *s = strings[i];
		while (*s != '\0') {
			putchar(toupper(*s));
			++s;
		}
		printf(" 0x%x\n", hash);
	}
}
