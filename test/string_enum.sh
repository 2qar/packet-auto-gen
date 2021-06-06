#!/bin/bash
# TODO: merge w/ build.sh

cd ../
make
./pc examples/string_enum.packet > /tmp/string_enum.h
cd test

CHOWDER_DIR="$(awk -F'= ' '{print $2}' ../config.mk)"
gcc -g -I/tmp -o /tmp/string_enum string_enum.c $CHOWDER_DIR/{conn,packet,nbt,player}.c $CHOWDER_DIR/include/linked_list.c -lssl -lcrypto

packet_file_path="$(/tmp/string_enum)"
diff string_enum.bin $packet_file_path || exit 1

rm $packet_file_path
rm /tmp/string_enum
