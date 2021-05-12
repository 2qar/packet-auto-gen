#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

// https://wiki.vg/Protocol#Data_types
// Values are (mostly) FNV-1a hashes for parsing
#define FT_BOOL 0x2f742c5d
#define FT_BYTE 0xcb39993f
#define FT_UBYTE 0xf9c13d6a
#define FT_SHORT 0x5fdddd75
#define FT_USHORT 0x182ca22
#define FT_INT 0xf87415fe
#define FT_LONG 0x45939473
#define FT_FLOAT 0x4c816225
#define FT_DOUBLE 0x8e464c28
#define FT_STRING 0x604f4858
#define FT_CHAT 0x2279d8cb
#define FT_IDENTIFIER 0x27ed05e
#define FT_VARINT 0x45d97a6f
#define FT_VARLONG 0xda7b82f8
#define FT_ENTITY_METADATA 0xd7d24cb7
#define FT_SLOT 0xf0c19391
#define FT_NBT_TAG 0x9da906ad
#define FT_POSITION 0xe27f342a
#define FT_ANGLE 0x39c493b8
#define FT_UUID 0x257b8832
//#define FT_OPTIONAL 15
#define FT_ARRAY 0x16c8fcc6
#define FT_ENUM 0xe84dda20
#define FT_BYTE_ARRAY 16

// These types aren't part of the protocol, but I need them for generating C structs
// and generating read/write code
#define FT_UNION 0xdbded6f4
#define FT_STRUCT 0x92c2be20
#define FT_STRUCT_ARRAY 17
#define FT_EMPTY 0xd1571f8e

struct field {
	uint32_t type;
	char *name;
	char *condition;
	union {
		size_t string_len;
		struct {
			uint32_t type;
			size_t constants_len;
			char **constants;
		} enum_data;
		struct {
			struct field *enum_field;
			struct field *fields;
		} union_data;
		struct field *fields;
		struct {
			uint32_t type;
			size_t array_len;
		} array;
	};
	struct field *next;
};

char *next_nonblank(char *);
struct token *read_field_type(struct token *, struct field *);
struct token *read_field_name(struct token *, struct field *);
struct token *read_conditional(struct token *, struct field *);
struct token *parse_enum(struct token *first_constant, struct field *field);
struct token *parse_field(struct token *, struct field *);

bool resolve_union_enums(struct field *root);

#endif // PARSER_H
