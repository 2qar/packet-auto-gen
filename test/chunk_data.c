#include <stdio.h>

#include "common.h"
#include "chunk_data.h"

#define BIOMES_LEN 1024
#define BIOME_PLAINS 1
#define BLOCKS_PER_SECTION 4096

int main()
{
	struct chunk_data_chunk_section section = {0};
	section.block_count = 16*16;
	section.bits_per_block = 4;
	section.palette_len = 2;
	section.palette = malloc(sizeof(int32_t) * 2);
	section.palette[0] = 0;
	section.palette[1] = 420;
	section.data_array_len = BLOCKS_PER_SECTION / ((sizeof(int64_t) * 8) / section.bits_per_block);
	section.data_array = calloc(section.data_array_len, sizeof(int64_t));

	section.data_array[0]  = 0x1111111111111111;
	section.data_array[1]  = 0x1111111111111111;
	section.data_array[2]  = 0x1111111111111111;
	section.data_array[3]  = 0x1111111111111111;
	section.data_array[4]  = 0x1111111111111111;
	section.data_array[5]  = 0x1111111111111111;
	section.data_array[6]  = 0x1111111111111111;
	section.data_array[7]  = 0x1111111111111111;
	section.data_array[8]  = 0x1111111111111111;
	section.data_array[9]  = 0x1111111111111111;
	section.data_array[10] = 0x1111111111111111;
	section.data_array[11] = 0x1111111111111111;
	section.data_array[12] = 0x1111111111111111;
	section.data_array[13] = 0x1111111111111111;
	section.data_array[14] = 0x1111111111111111;
	section.data_array[15] = 0x1111111111111111;

	struct chunk_data chunk = {0};
	chunk.chunk_x = 2;
	chunk.chunk_z = 5;
	chunk.full_chunk = true;
	chunk.primary_bit_mask = 0x1;
	chunk.heightmaps = nbt_new(TAG_Long_Array, "MOTION_BLOCKING");
	chunk.heightmaps->data.array = malloc(sizeof(struct nbt_array));
	chunk.heightmaps->data.array->len = 36;
	chunk.heightmaps->data.array->data.longs = calloc(36, sizeof(int64_t));
	chunk.biomes = malloc(sizeof(int32_t) * BIOMES_LEN);
	for (size_t i = 0; i < BIOMES_LEN; ++i)
		chunk.biomes[i] = BIOME_PLAINS;
	chunk.data_len = 1;
	chunk.data = &section;

	struct test t = {0};
	test_init(&t);
	if (t.conn == NULL)
		return 1;

	struct protocol_err r = protocol_write_chunk_data(t.conn->packet, &chunk);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	printf("%s\n", t.packet_file_path);
	test_cleanup(&t);
	return 0;
}