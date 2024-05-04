CC=gcc
CFLAGS=-Wall -Wextra

.PHONY: all clean

all: wad-demul

wad-demul: main.c
	$(CC) $(CFLAGS) -O3 -o $@ $^

clean:
	rm -rf wad-demul
