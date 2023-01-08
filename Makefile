# Initial config
LIBS=
SRC=$(wildcard src/*.c)
SRC+=test.c

override CFLAGS?=
override CFLAGS+=-Wall
override CFLAGS+=-Isrc

# Which objects to generate before merging everything together
OBJ:=$(SRC:.c=.o)

default: test

# How to fetch libraries
$(LIBS):
	git submodule update --init $@

%.o: %.c $(LIBS)
	$(CC) $(CFLAGS) $(@:.o=.c) -c -o $@

test: $(OBJ) test.h
	$(CC) $(CFLAGS) $(OBJ) -o $@

.PHONY: clean
clean:
	rm -f $(OBJ)
