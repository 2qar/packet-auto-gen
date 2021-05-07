CC=gcc
CFLAGS=-g -Wall -Werror -Wextra -pedantic
TARGET=pc

$(TARGET): main.o parser.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm $(TARGET) *.o
