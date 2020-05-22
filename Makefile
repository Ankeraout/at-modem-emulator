CC=cc -c
CFLAGS=-W -Wall -Wextra -std=gnu11 -pedantic
LD=cc
LDFLAGS=-lpthread

BINDIR=bin

SOURCES=src/modem.c src/client.c src/protocols/ppp.c src/protocols/lcp.c src/protocols/ipcp.c src/protocols/ipv4.c
OBJECTS=$(SOURCES:%.c=%.o)
EXEC=$(BINDIR)/modem

ifeq ($(MODE),)
    MODE = release
endif

ifeq ($(MODE), debug)
	CFLAGS += -DDEBUG -O0 -g
	LDFLAGS += -g
else
	CFLAGS += -O3 -march=native
	LDFLAGS += -s
endif

CFLAGS += -I`pwd`/src

all: $(EXEC)

$(BINDIR):
	mkdir $(BINDIR)

$(EXEC): $(OBJECTS) bin
	$(LD) $(OBJECTS) -o $(EXEC) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJECTS) $(BINDIR)

.PHONY: clean all
