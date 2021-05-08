CC=gcc
CFLAGS=-g -Wall -Werror -Wextra -pedantic
TARGET=pc

$(TARGET): main.o lexer.o parser.o
	$(CC) $(CFLAGS) -o $@ $^

parser.o: lexer.o

clean:
	rm $(TARGET) *.o
