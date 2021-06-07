#include <stdio.h>
#include "conn.h"
#include "packet.h"

struct test {
	int packet_fd;
	char *packet_file_path;
	struct conn *conn;
};

void test_init(struct test *);
void test_cleanup(struct test *);
