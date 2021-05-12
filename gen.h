#include "parser.h"

void put_includes();
void generate_struct(const char *name, struct field *fields);
void generate_write_function(const char *name, struct field *fields);
