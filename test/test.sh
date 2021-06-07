#!/bin/bash
# Build + run a test and check the output.
# maybe this file should be called 'test.sh' because it does a teeny bit more
# than just build

script_name="$0"
packet_name="$1"

err () {
	echo "$script_name: $1" 2>&1
	exit 1
}

if [ "$(basename $(pwd))" != "test" ]; then
	err "must be run in test/"
fi

if [ -z "$packet_name" ]; then
	err "empty test name"
elif ! [ -r "$packet_name.c" ]; then
	err "\"$packet_name.c\" doesn't exist / isn't readable"
elif ! [ -r "$packet_name.bin" ]; then
	err "\"$packet_name.bin\" doesn't exist / isn't readable"
fi

if ! [ -x "../pc" ]; then
	err "../pc doesn't exist / isn't executable"
fi
../pc ../examples/$packet_name.packet > /tmp/$packet_name.h

chowder_dir="$(awk -F'= ' '{print $2}' ../config.mk)"
gcc -g -I/tmp -I$chowder_dir -o /tmp/$packet_name $packet_name.c bin/*.o -lssl -lcrypto || exit 1

packet_file_path="$(/tmp/$packet_name)"
diff $packet_name.bin $packet_file_path || exit 1

rm $packet_file_path
rm /tmp/$packet_name
