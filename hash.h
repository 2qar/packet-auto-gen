#ifndef PACKET_HASH
#define PACKET_HASH
#include <stdint.h>

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV-1a_hash
uint32_t str_fnv1a(const char *s)
{
	uint32_t hash = 0x811c9dc5;
	while (*s != '\0') {
		hash ^= *s;
		hash *= 0x01000193;
		++s;
	}
	return hash;
}

#endif
