dkparse: dkparse.y
	bison -v dkparse.y
	cc -Wall -o dkparse dkparse.tab.c
