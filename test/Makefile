include ../config.mk

CC=gcc
CPPFLAGS=-I`pwd` -I$(CHOWDER_DIR)
CFLAGS=-g -Wall -Werror -Wextra -pedantic

OBJDIR=bin
CHOWDER_OBJECTS=conn.o packet.o nbt.o player.o
CHOWDER_INCLUDE_OBJECTS=linked_list.o
OBJECTS=common.o
ALL_OBJECTS=$(OBJECTS) $(CHOWDER_OBJECTS) $(CHOWDER_INCLUDE_OBJECTS)

deps: $(addprefix $(OBJDIR)/,$(ALL_OBJECTS))

$(addprefix $(OBJDIR)/,$(OBJECTS)): $(OBJDIR)/%.o: %.c
	$(CC) $(CPPFLAGS) -c $(CFLAGS) $< -o $@

$(addprefix $(OBJDIR)/,$(CHOWDER_OBJECTS)): $(OBJDIR)/%.o: $(CHOWDER_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(addprefix $(OBJDIR)/,$(CHOWDER_INCLUDE_OBJECTS)): $(OBJDIR)/%.o: $(CHOWDER_DIR)/include/%.c
	$(CC) -c $(CFLAGS) $< -o $@
