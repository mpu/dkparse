#include <stdio.h>
#include "dk.h"

/* struct L - The datatype used to store environments.
 */
struct L {
	char *s;
	struct L *n;
};

/* struct Env - Environments are pairs of an identifier
 * and a term (its type), we use a nested struct L in order
 * to be able to cast a pointer to struct B into a pointer
 * to struct L and vice versa.
 * The 'last' pointer is used to keep track of the last
 * inserted element in the list.
 */
struct Env {
	struct B {
		struct L l;
		struct Term *t;
	} *b;
	struct B *last;
};

/* internal cons - Add a value to the current environment.
 */
static inline void
cons(char *s, struct L **l)
{
	struct L *car=xalloc(sizeof *car);
	car->s=s;
	car->n=*l;
	*l=car;
}

/* internal tscope -
 */
static int
tscope(struct Term *t, struct L *pe)
{
	int r=0;
	struct L *p, *e=pe;

tail:
	switch (t->typ) {
	case App:
		r=tscope(t->uapp.t1, e);
		if (r) break;
		t=t->uapp.t2;
		goto tail;
		break;
	case Lam:
		cons(t->ulam.x, &e);
		t=t->ulam.t;
		goto tail;
		break;
	case Pi:
		r=tscope(t->upi.ty, e);
		if (r) break;
		cons(t->upi.x, &e);
		t=t->upi.t;
		goto tail;
		break;
	case Var:
		for (p=e; p; p=p->n)
			if (p->s==t->uvar)
				goto ret;
		fprintf(stderr, "Variable %s is out of scope.\n", t->uvar);
		r=1;
		break;
	case Type:
		break;
	}
ret:
	p=e;
	while (p!=pe) {
		struct L *tmp;
		tmp=p->n;
		free(p);
		p=tmp;
	}
	return r;
}

/* scope -
 */
int
scope(struct Term *t)
{
	return tscope(t, 0);
}

/* ------------- Rule environments handling. ------------- */

/* enew - Returns a fresh empty environment. The environment
 * cell is allocated on the temporary heap.
 */
struct Env *
enew(void)
{
	struct Env *e=dkalloc(sizeof *e);

	e->b=0;
	e->last=0;
	return e;
}

/* eins - Insert a binding in an environment. The environment
 * cell is allocated on the temporary heap.
 */
void
eins(struct Env *e, char *id, struct Term *ty)
{
	struct B *b=dkalloc(sizeof *b);

	b->l.s=id;
	b->l.n=0;
	b->t=ty;

	if (!e->last)
		e->b=b;
	else
		e->last->l.n=&b->l;
	e->last=b;
}

/* eiter - Iterate a function on an environment.
 */
void
eiter(struct Env *e, void (*f)(char *, struct Term *))
{
	struct B *b=e->b;

	while (b) {
		f(b->l.s, b->t);
		b=(struct B *)b->l.n;
	}
}
