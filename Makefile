#
# \file Makefile
# \author Martin Weise <e1429167@student.tuwien.ac.at>
# \date 28.11.2016
# \brief The Makefile.
# \details Add -DENDEBUG to \p DEFS to enable debugging.
# \info Add -lrt on _end_ of compilation line

CC=gcc
DEFS=-D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS=-Wall -g -std=c99 -pedantic -lm -lcrypto -pthread $(DEFS)
LDFLAGS=-lrt -lpthread

all: src/auth-server src/auth-client

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/auth-server: src/auth-server.o src/shared.o
	$(CC) -o $@ $^ $(LDFLAGS)

src/auth-client: src/auth-client.o src/shared.o
	$(CC) -o $@ $^ $(LDFLAGS)

test: all
	sh test/test.sh

doxygen:
	doxygen doc/Doxyfile

clean:
	rm -f src/auth-server src/auth-server.o src/auth-client src/auth-client.o
	rm -f /dev/shm/1429167fragment /dev/shm/sem.1429167sem*

.PHONY: clean