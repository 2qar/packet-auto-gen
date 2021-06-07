#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "string_enum.h"

int main()
{
	struct string_enum string_enum = {0};
	string_enum.level_type = "default";
	string_enum.test = 1;

	struct test t = {0};
	test_init(&t);
	if (t.conn == NULL)
		return 1;

	int status = protocol_write_string_enum(t.conn, &string_enum);
	if (status < 0)
		return 1;

	printf("%s\n", t.packet_file_path);
	test_cleanup(&t);
	return 0;
}
