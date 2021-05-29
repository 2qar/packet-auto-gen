#!/bin/bash
# Build + run a test and check the output.
# maybe this file should be called 'test.sh' because it does a teeny bit more
# than just build

# TODO: terminate when make or gcc fail
# TODO: take a packet name, eg. "player_info", and plug that name in below
#       so this can be used for any test

cd ../
make
./pc examples/player_info.packet > /tmp/player_info.h
cd test

CHOWDER_DIR="$(awk -F'= ' '{print $2}' ../config.mk)"
gcc -g -I/tmp -o /tmp/player_info player_info.c $CHOWDER_DIR/{conn,packet,nbt,player}.c $CHOWDER_DIR/include/linked_list.c -lssl -lcrypto

packet_file_path="$(/tmp/player_info)"
diff player_info.bin $packet_file_path || exit 1

rm $packet_file_path
rm /tmp/player_info
