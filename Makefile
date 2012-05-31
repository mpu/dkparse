CFILES = lib/avl.c alloc.c term.c pat.c rule.c dkparse.tab.c scope.c module.c gen.c
OFILES = $(CFILES:.c=.o)

dkparse: $(OFILES)
	cc -p -o dkparse $(OFILES)

dkparse.tab.c: dkparse.y dk.h
	bison dkparse.y

.c.o:
	cc -p -Wall -std=c99 -g -o $@ -c $<

$(OFILES): dk.h

.PHONY: stat
SOURCES=lib/avl.c alloc.c term.c pat.c rule.c scope.c module.c gen.c
stat:
	c_count ${SOURCES}

.PHONY: test
test:
	lua test/do.lua -p `pwd`
