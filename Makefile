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
	makeinfo doc/dedukti.texinfo -o doc/dedukti.info
	gzip doc/dedukti.info
	makeinfo --no-split --html doc/dedukti.texinfo -o doc/dedukti.html

install:
	install -m 755 dkparse $(BIN)/dkparse
	ln -f $(BIN)/dkparse $(BIN)/dedukti
	mkdir -p $(LUALIB)
	install -m 644 lua/dedukti.lua $(LUALIB)/dedukti.lua
	if [ -e doc/dedukti.info.gz ]; then \
	  install -m 644 doc/dedukti.info.gz $(INFO)/dedukti.info.gz; \
	fi
