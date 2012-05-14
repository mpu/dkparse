CFILES = lib/avl.c alloc.c term.c dkparse.tab.c scope.c
OFILES = $(CFILES:.c=.o)

dkparse: $(OFILES)
	cc -o dkparse $(OFILES)

dkparse.tab.c: dkparse.y dk.h
	bison -v dkparse.y

.c.o:
	cc -Wall -std=c99 -g -o $@ -c $<

$(OFILES): dk.h
