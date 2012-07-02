# Installation settings
LUALIB = /usr/share/lua/5.1
BIN = /usr/bin

# Compilation
CFILES = lib/avl.c alloc.c term.c pat.c rule.c dkparse.tab.c scope.c module.c gen.c
OFILES = $(CFILES:.c=.o)

dkparse: $(OFILES)
	cc -p -o dkparse $(OFILES)

dkparse.tab.c: dkparse.y dk.h
	bison dkparse.y

.c.o:
	cc -p -Wall -std=c99 -g -o $@ -c $<

$(OFILES): dk.h

.PHONY: stat test install

SOURCES=lib/avl.c alloc.c term.c pat.c rule.c scope.c module.c gen.c
stat:
	c_count ${SOURCES}

test:
	lua test/do.lua -p `pwd`

install:
	install -m 644 lua/dedukti.lua $(LUALIB)/dedukti.lua
	install -m 755 dkparse $(BIN)/dkparse
	ln $(BIN)/dkparse $(BIN)/dedukti
