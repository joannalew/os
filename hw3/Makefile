CC=clang
CFLAGS=-g -Wall -std=c99

all:
	${CC} smallsh.c -o smallsh ${CFLAGS} -D_XOPEN_SOURCE

clean:
	rm -f smallsh
	rm -rf *.dSYM