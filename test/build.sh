#!/bin/bash
# Build + run a test and check the output.
# maybe this file should be called 'test.sh' because it does a teeny bit more
# than just build

# TODO: terminate when make or gcc fail

CHOWDER_DIR="$(awk -F'= ' '{print $2}' ../config.mk)"

script_name="$0"
packet_name="$1"

err () {
	echo "$script_name: $1" 2>&1
	exit 1
}

if [ -z "$packet_name" ]; then
	err "empty test name"
elif ! [ -r "$packet_name.c" ]; then
	err "\"$packet_name.c\" doesn't exist / isn't readable"
elif ! [ -r "$packet_name.bin" ]; then
	err "\"$packet_name.bin\" doesn't exist / isn't readable"
fi

cd ../
make
./pc examples/$packet_name.packet > /tmp/$packet_name.h
cd test

gcc -g -I/tmp -o /tmp/$packet_name $packet_name.c $CHOWDER_DIR/{conn,packet,nbt,player}.c $CHOWDER_DIR/include/linked_list.c -lssl -lcrypto

packet_file_path="$(/tmp/$packet_name)"
diff $packet_name.bin $packet_file_path || exit 1

rm $packet_file_path
rm /tmp/$packet_name
