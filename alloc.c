#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dk.h"
#define POOLSZ  4096
#define APOOLSZ 1024

/* One memory pool is kept to store values of allocated
 * pointers, during one translation phase memory is never
 * freed, at the end of the translation, the whole pool is
 * released.
 */
static void **pool;
static size_t pi, psz;

/* Fast string management with sharing is achieved using a
 * hash table to store all strings (symbols) used during
 * the parsing.
 */
struct Atom {
	char *s;
	struct Atom *next;
} **apool;

/* xalloc - A safe memory allocator, will exit if we run
 * out of memory.
 */
void *
xalloc(size_t s)
{
	void *p = malloc(s);
	if (!p) {
		fprintf(stderr, "We are out of memory.\n"
		                "\tSize of the current pool: %zuk\n"
		              , pi/1024);
		abort();
	}
	return p;
}

/* xrealloc - A safe memory allocator.
 */
void *
xrealloc(void *p, size_t s)
{
	p=realloc(p, s);
	if (!p) {
		fprintf(stderr, "We are out of memory.\n"
		                "\tSize of the current pool: %zuk\n"
		              , pi/1024);
		abort();
	}
	return p;
}

/* initalloc - Initialize the gobal memory pool and the atom
 * hash table.
 */
void
initalloc(void)
{
	int i;

	psz=POOLSZ;
	pool=xalloc(psz*sizeof *pool);
	apool=xalloc(APOOLSZ*sizeof *apool);
	for (i=0; i<APOOLSZ; i++)
		apool[i]=0;
}

/* deinitalloc - Free the global memory pool and the atom
 * hash table.
 */
void
deinitalloc(void)
{
	int i;

	free(pool);
	for (i=0; i<APOOLSZ; i++) {
		struct Atom *t, *p=apool[i];
		while (p) {
			t=p->next;
			free(p->s);
			free(p);
			p=t;
		}
	}
	free(apool);
}

/* internal hash - Simple hash of a string.
 */
static inline unsigned
hash(const char *s)
{
	unsigned h;
	for (; *s; s++)
		h = h*19 + (unsigned)*s;
	return h;
}

/* astrdup - Return an atom string allocated on the heap equal to
 * the string passed as argument.
 */
char *
astrdup(const char *s)
{
	struct Atom **p;

	p=&apool[hash(s)%APOOLSZ];
	while (*p && strcmp((*p)->s, s))
		p=&(*p)->next;
	if (*p)
		return (*p)->s;
	*p=xalloc(sizeof **p);
	(*p)->s=xalloc(strlen(s)+1);
	(*p)->next=0;
	return strcpy((*p)->s, s);
}

/* dkalloc - Allocate one memory block in the pool.
 */
void *
dkalloc(size_t s)
{
	if (pi>=psz) {
		psz*=2;
		fprintf(stderr, "Pool too small, allocating %zu entries.\n"
		              , psz);
		pool=xrealloc(pool, psz*sizeof *pool);
	}
	pool[pi] = xalloc(s);
	return pool[pi++];
}

/* dkfree - Free the temporary memory pool.
 */
void
dkfree(void)
{
	while (pi--)
		free(pool[pi]);
	pi=0;
}
