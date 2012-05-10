CFILES = alloc.c term.c dkparse.tab.c
OFILES = $(CFILES:.c=.o)

dkparse: $(OFILES) dk.h
	cc -o dkparse $(OFILES)

dkparse.tab.c: dkparse.y
	bison -v dkparse.y

.c.o:
	cc -Wall -std=c99 -g -c $<
