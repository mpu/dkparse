#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dk.h"
#define POOLSZ 4096

/* One memory pool is kept to store values of allocated
 * pointers, during one translation phase memory is never
 * freed, at the end of the translation, the whole pool is
 * released.
 */
static void **pool;
static size_t pi, psz;

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

/* initalloc - Initialize the gobal memory pool.
 */
void
initalloc(void)
{
	psz=POOLSZ;
	pool=xalloc(psz*sizeof *pool);
}

/* deinitalloc - Free the global memory pool.
 */
void
deinitalloc(void)
{
	free(pool);
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

/* dkstrdup - A safe string copy function. Warning, it uses
 * the memory pool, and is freed at each new compiling step.
 */
char *
dkstrdup(const char *s)
{
	char *t = dkalloc(strlen(s)+1);
	return strcpy(t, s);
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

