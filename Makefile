CC=gcc
CFLAGS=-g -Wall -Werror -Wextra -pedantic
TARGET=pc

$(TARGET): main.o lexer.o parser.o
	$(CC) $(CFLAGS) -o $@ $^

parser.o: lexer.o

fnv-util:
	$(CC) $(CFLAGS) -o $@ fnv-util.c

clean:
	rm $(TARGET) *.o
