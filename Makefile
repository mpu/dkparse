# Installation settings
LUALIB = /usr/share/lua/5.1
BIN = /usr/bin
INFO = /usr/share/info

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

.PHONY: stat test doc install

SOURCES=lib/avl.c alloc.c term.c pat.c rule.c scope.c module.c gen.c
stat:
	c_count ${SOURCES}

test:
	lua test/do.lua -p `pwd`

doc:
	makeinfo doc/dkparse.texinfo -o doc/dkparse.info
	gzip doc/dkparse.info
	makeinfo --no-split --html doc/dkparse.texinfo -o doc/dkparse.html

install:
	install -m 644 lua/dedukti.lua $(LUALIB)/dedukti.lua
	install -m 755 dkparse $(BIN)/dkparse
	ln -f $(BIN)/dkparse $(BIN)/dedukti
	[ -e doc/dkparse.info.gz ] && \
	  install -m 644 doc/dkparse.info.gz $(INFO)/dkparse.info.gz
