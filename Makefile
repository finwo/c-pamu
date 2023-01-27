# Initial config
LIBS=
SRC=$(wildcard src/*.c)
SRC+=test.c

CFLAGS:=
CFLAGS+=-Wall

INCLUDES:=
INCLUDES+=-I src

include lib/.dep/config.mk

CFLAGS+=$(INCLUDES)

# Which objects to generate before merging everything together
OBJ:=$(SRC:.c=.o)

default: test

# How to fetch libraries
$(LIBS):
	git submodule update --init $@

%.o: %.c $(LIBS)
	$(CC) $(CFLAGS) $(@:.o=.c) -c -o $@

test: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

.PHONY: clean
clean:
	rm -f $(OBJ)
