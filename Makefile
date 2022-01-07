CC=gcc
CPPFLAGS=-Iinclude
CFLAGS=-g -Wall -Werror -Wextra -pedantic
TARGET=pc

$(TARGET): main.o lexer.o parser.o gen.o
	$(CC) $(CFLAGS) -o $@ $^

parser.o: lexer.o

gen.o: parser.o

fnv-util:
	$(CC) $(CFLAGS) -o $@ fnv-util.c

clean:
	rm $(TARGET) *.o
