#include <stdio.h>
#include "dk.h"

/* struct L - The datatype used to store environments.
 */
struct L {
	char *s;
	struct L *n;
};

/* internal cons -
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
