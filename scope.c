#include <stdio.h>
#include <string.h>
#include "dk.h"
#include "lib/avl.h"

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

/* struct Id - A identifier is a name declared in a dedukti
 * file, it can have sevaral statuses: DECL, is this case
 * the identifier was declared but no rewrite rules were given;
 * DEF, in this case the identifier was declared and rewrite
 * rules were given to define its behavior.
 */
struct Id {
	char *x;
	enum IdStatus st;
};

/* genv - The global environment.
 */
extern struct Tree *genv;

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
	struct Id id;

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
		id.x=t->uvar;
		if (avlget(&id, genv))
			goto ret;
		fprintf(stderr, "%s: Variable %s is out of scope.\n", __func__, t->uvar);
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

/* ------------- Global environment handling. ------------- */

struct Tree *genv;

/* internal idcmp - Compare to identifiers, this is used by
 * the avl tree implementation.
 */
static int
idcmp(void *a, void *b)
{
	return strcmp(((struct Id *)a)->x, ((struct Id *)b)->x);
}

/* initscope - Initialize the global scope environment.
 */
void
initscope(void)
{
	genv=avlnew(idcmp, free);
}

/* deinitscope - Free the global environment and all its
 * contents.
 */
void
deinitscope(void)
{
	avlfree(genv);
	genv=0;
}

/* pushscope - Add an identifier to the global scope, by default
 * this identifier will be in DEF state. It will display an error
 * message if the identifier is already in the global scope.
 */
void
pushscope(char *x)
{
	struct Id *id=xalloc(sizeof *id);

	id->x=x;
	id->st=DEF;
	if (avlget(id, genv)) {
		fprintf(stderr, "%s: Id %s already declared.\n", __func__, x);
		exit(1); // FIXME
	}
	avlins(id, genv);
}

/* chscope - Change the status of an identifier already in scope.
 * The old status is returned.
 */
enum IdStatus
chscope(char *x, enum IdStatus st)
{
	enum IdStatus old;
	struct Id *id, idx = { x };

	id=avlget(&idx, genv);
	if (!id) {
		fprintf(stderr, "%s: Id %s is out of scope.\n", __func__, x);
		exit(1); // FIXME
	}
	old=id->st;
	id->st=st;
	return old;
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
