CFILES = lib/avl.c alloc.c term.c rule.c dkparse.tab.c scope.c gen.c module.c
OFILES = $(CFILES:.c=.o)

dkparse: $(OFILES)
	cc -p -o dkparse $(OFILES)

dkparse.tab.c: dkparse.y dk.h
	bison -v dkparse.y

.c.o:
	cc -p -Wall -std=c99 -g -o $@ -c $<

$(OFILES): dk.h
