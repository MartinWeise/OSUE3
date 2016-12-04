#
# \file Makefile
# \author Martin Weise <e1429167@student.tuwien.ac.at>
# \date 28.11.2016
# \brief The Makefile.
# \details Add -DENDEBUG to \p DEFS to enable debugging.
#

CC=gcc
DEFS=-D_XOPEN_SOURCE=500 -D_BSD_SOURCE -DENDEBUG
CFLAGS=-Wall -g -std=c99 -pedantic $(DEFS)
LDFLAGS=
#-lcrypt

all: src/auth-server src/auth-client

# COMPILE

src/auth-client: src/auth-client.o
	$(CC) $(LDFLAGS) -o $@ $^

src/auth-client.o: src/auth-client.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/auth-server: src/auth-server.o
	$(CC) $(LDFLAGS) -o $@ $^

src/auth-server.o: src/auth-server.c
	$(CC) $(CFLAGS) -c -o $@ $<

# DEBUG

clean:
	rm -f src/auth-server src/auth-server.o src/auth-client src/auth-client.o

.PHONY: clean