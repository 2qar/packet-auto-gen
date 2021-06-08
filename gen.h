#include "parser.h"

void put_id(const char *packet_filename, int id);
void put_includes();
void generate_structs(const char *name, struct field *fields);
void generate_write_function(int id, const char *name, struct field *fields);
void generate_read_function(int id, const char *name, struct field *fields);
