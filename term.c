#include "dk.h"

struct Term *ttype = &(struct Term){ Type };

/* mkapp - Build an application node.
 */
struct Term *
mkapp(struct Term *a, struct Term *b)
{
	struct Term *t = dkalloc(sizeof *t);
	t->typ = App;
	t->uapp.t1 = a;
	t->uapp.t2 = b;
	return t;
}

/* mkvar - Build a variable node.
 */
struct Term *
mkvar(char *s)
{
	struct Term *t = dkalloc(sizeof *t);
	t->typ = Var;
	t->uvar = s;
	return t;
}

/* mklam - Build a lambda node.
 */
struct Term *
mklam(char *s, struct Term *a)
{
	struct Term *t = dkalloc(sizeof *t);
	t->typ = Lam;
	t->ulam.x = s;
	t->ulam.t = a;
	return t;
}

/* mkpi - Build a Pi node.
 */
struct Term *
mkpi(char *s, struct Term *ty, struct Term *a)
{
	struct Term *t = dkalloc(sizeof *t);
	t->typ = Pi;
	t->upi.x = s;
	t->upi.ty = ty;
	t->upi.t = a;
	return t;
}

/* napps - Peel a succession of applications and return
 * the number of terms which are arguments in the spine.
 * If the p pointer is not null, the leftmost term in the
 * spine is assigned to the object pointed by p.
 */
int
napps(struct Term *t, struct Term **p)
{
	int r;

	for (r=0; t->typ==App; t=t->uapp.t1)
		r++;
	if (p)
		*p=t;
	return r;
}

/* iskind - Checks if a term is a kind, we can check this
 * syntactically: the term is a kind if it is a succession
 * of products ending on Type.
 */
int
iskind(struct Term *t)
{
	while (t->typ==Pi)
		t=t->upi.t;
	return t==ttype;
}
