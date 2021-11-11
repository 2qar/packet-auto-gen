#!/bin/bash
# Build + run a test and check the output.

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
output_dir="/tmp"
../pc -o $output_dir ../examples/$packet_name.packet
chowder_dir="$(awk -F'= ' '{print $2}' ../config.mk)"
gcc -I/tmp -I../include -I$chowder_dir -DPACKET_FILE_PATH="\"/tmp/$packet_name.bin\"" \
	-g -Wall -Wextra -Werror -pedantic \
	-o /tmp/$packet_name \
	$packet_name.c bin/*.o \
	$output_dir/$packet_name.c \
	-lssl -lcrypto || exit 1
packet_file_path=$(/tmp/$packet_name) || err "$packet_name failed"
diff $packet_name.bin $packet_file_path || exit 1

rm $output_dir/$packet_name.{c,h}
rm $packet_file_path
rm /tmp/$packet_name
