SRC=$(wildcard *.c)
BIN=$(SRC:.c=)

CFLAGS:=
CFLAGS+=-Wall
CFLAGS+=-I../src

default: $(BIN) .gitignore

$(BIN): $(SRC) ../src/pamu.c
	$(CC) $(CFLAGS) $@.c ../src/pamu.c -o $@

.gitignore:
	echo $(BIN) | tr ' ' '\n' > .gitignore
	echo example.pamu >> .gitignore

.PHONY: clean
clean:
	rm -f $(BIN)
