#ifndef CHOWDER_PROTOCOL_TYPES_H
#define CHOWDER_PROTOCOL_TYPES_H

enum protocol_err_type {
	PROTOCOL_ERR_SUCCESS,
	PROTOCOL_ERR_PACKET,
	PROTOCOL_ERR_INPUT,
};

enum protocol_input_err_type {
	PROTOCOL_INPUT_ERR_RANGE,
	PROTOCOL_INPUT_ERR_VARINT_RANGE,
	PROTOCOL_INPUT_ERR_LEN,
	PROTOCOL_INPUT_ERR_BAD_ENUM_CONSTANT,
};

struct protocol_err {
	enum protocol_err_type err_type;
	union {
		int packet_err;
		struct {
			const char *field_name;
			enum protocol_input_err_type err_type;
			union {
				struct {
					size_t min;
					size_t max;
					size_t value;
				} range;
				struct {
					size_t max;
					size_t value;
				} len;
			};
			char *enum_constant;
		} input_err;
	};
};

#endif // CHOWDER_PROTOCOL_TYPES_H