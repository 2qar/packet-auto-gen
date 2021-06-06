#include <stdio.h>
#include <stdlib.h>

#include "string_enum.h"

int main()
{
	struct string_enum string_enum = {0};
	string_enum.level_type = "default";
	string_enum.test = 1;

	char path[] = "/tmp/packet-XXXXXX";
	mkstemp(path);
	FILE *f = fopen(path, "w");
	if (f == NULL) {
		perror("fopen");
		return 1;
	}

	struct packet p;
	packet_init(&p);

	struct conn c = {0};
	c.packet = &p;
	c.sfd = fileno(f);

	int status = protocol_write_string_enum(&c, &string_enum);
	free(p.data);
	fclose(f);
	if (status < 0)
		return 1;

	printf("%s\n", path);
	return 0;
}
