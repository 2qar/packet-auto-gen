#!/bin/bash

err() {
	echo "$0: $1" 2>&1
	exit 1
}

if [ "$(basename $(pwd))" != "test" ]; then
	err "must be run in test/"
fi

cd ../
make
cd test

for filename in $(ls -1 *.c); do
	packet_name=$(echo "$filename" | sed 's/\.c//g')
	./test.sh "$packet_name"
done
