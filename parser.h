// https://wiki.vg/Protocol#Data_types
// Values are FNV-1a hashes for parsing
enum field_type {
	FT_BOOL = 0xc894953d,
	FT_BYTE,
	FT_UBYTE,
	FT_SHORT,
	FT_USHORT,
	FT_INT,
	FT_LONG,
	FT_FLOAT,
	FT_DOUBLE,
	FT_STRING = 0x604f4858,
	FT_CHAT = 0x2279d8cb,
	FT_IDENTIFIER,
	FT_VARINT = 0x45d97a6f,
	FT_VARLONG,
	FT_ENTITY_METADATA,
	FT_SLOT,
	FT_NBT_TAG,
	FT_POSITION,
	FT_ANGLE,
	FT_UUID = 0x257b8832,
	FT_OPTIONAL,
	FT_ARRAY = 0x16c8fcc6,
	FT_ENUM = 0xe84dda20,
	FT_BYTE_ARRAY,

	// not part of the protocol, but i need them for generating C-structs
	FT_UNION,
	FT_STRUCT = 0x92c2be20,
	FT_STRUCT_ARRAY,
	FT_EMPTY = 0xd1571f8e,
};

struct field {
	enum field_type type;
	char *name;
	union {
		size_t string_len;
		struct {
			enum field_type type;
			size_t constants_len;
			char **constants;
		} enum_data;
		struct {
			char *enum_field_name;
			struct field_list *fields;
		} union_data;
		struct field_list *fields;
		struct {
			enum field_type type;
			size_t array_len;
		} array;
		struct {
			char *condition;
			struct field *field;
		} optional;
	};
};

struct field_list {
	struct field *field;
	struct field *next;
};

struct field *parse_enum(struct token **t);
