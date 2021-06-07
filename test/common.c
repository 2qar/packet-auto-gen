#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

void test_init(struct test *t)
{
	t->packet_file_path = strdup("/tmp/packet-XXXXXX");
	t->packet_fd = mkstemp(t->packet_file_path);
	if (t->packet_fd < 0) {
		perror("mkstemp");
		return;
	}

	t->conn = calloc(1, sizeof(struct conn));
	t->conn->sfd = t->packet_fd;
	t->conn->packet = malloc(sizeof(struct packet));
	packet_init(t->conn->packet);
}

void test_cleanup(struct test *t)
{
	close(t->packet_fd);
	free(t->packet_file_path);
	if (t->conn != NULL) {
		packet_free(t->conn->packet);
		free(t->conn);
	}
}
