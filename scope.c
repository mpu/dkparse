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
 * to be able to cast a pointer to struct Env into a pointer
 * to struct L and vice versa.
 * The p pointer is used to have a doubly linked list.
 * The list defined by l.n is a valid simply linked list,
 * while the one defined by p is looped.
 * Here is a small diagram that explains it.
 *
 *     __ n  __    __    __
 *    |  |->|  |->|  |->|  |->0
 *  ,-|__|<-|__|<-|__|<-|__|<-.
 *  \________________________/
 *               p
 */
struct Env {
	struct L l;
	struct Term *t;
	struct Env *p;
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

/* internal tscp - Check that a term is well scoped within
 * the global environment and the local pe environment. This
 * will also qualify all unbound names with the current module
 * name.
 */
static int
tscp(struct Term *t, struct L *pe)
{
	int r=0;
	struct L *p, *e=pe;
	struct Id id;

tail:
	switch (t->typ) {
	case App:
		r=tscp(t->uapp.t1, e);
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
		r=tscp(t->upi.ty, e);
		if (r) break;
		if (t->upi.x)
			cons(t->upi.x, &e);
		t=t->upi.t;
		goto tail;
		break;
	case Var:
		for (p=e; p; p=p->n)
			if (p->s==t->uvar)
				goto ret;
		if (aqual(t->uvar)) /* XXX Temporary hack to handle modules. */
			goto ret;
		t->uvar=mqual(t->uvar);
		id.x=t->uvar;
		if (avlget(&id, genv))
			goto ret;
		fprintf(stderr, "%s: Variable %s is out of scope.\n", __func__, id.x);
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

/* tscope - Scope a term in the global environment plus the given
 * environment, it will also qualify all names that appear unbound.
 * If the term is not well scoped, 1 is returned, 0 otherwise.
 */
int
tscope(struct Term *t, struct Env *e)
{
	return tscp(t, (struct L *)e);
}

/* internal penv - Store the environment when scoping patterns.
 * Since patterns cannot contain binders, the environment can
 * be shared between all recursive calls.
 */
static struct L *penv;

/* internal pscp - Scope a pattern, in a recursive fashion.
 */
static int
pscp(struct Pat *p)
{
	int i;
	struct L *e;
	struct Id id;

tail:
	for (e=penv; e; e=e->n)
		if (e->s==p->c) {
			if (p->nd+p->np==0) {
				p->np=-1;
				return 0;
			}
			fprintf(stderr, "%s: Pattern variable %s must not"
					" be applied.\n", __func__, p->c);
			return 1;
		}
	if (aqual(p->c)) /* XXX Temporary hack to handle modules. */
		goto scopechild;
	p->c=mqual(p->c); /* Not in local scope, qualify it. */
	id.x=p->c;
	if (avlget(&id, genv))
		goto scopechild;
	fprintf(stderr, "%s: Constructor %s is out of scope.\n", __func__, id.x);
	return 1;

scopechild:
	for (i=0; i<p->nd; i++)
		if (tscp(p->ds[i], penv))
			return 1;
	if (p->np==0)
		return 0;
	for (i=0; i<p->np-1; i++)
		if (pscp(p->ps[i]))
			return 1;
	p=p->ps[i];
	goto tail;
}

/* pscope - Scope a pattern in the global environment plus the
 * given environment, it will also qualify all names that
 * appear unbound and mark variables with a negative nd. If the
 * pattern is not well scoped or if it contains an applied pattern
 * variable, 1 is returned, 0 otherwise.
 */
int
pscope(struct Pat *p, struct Env *e)
{
	penv=(struct L *)e;
	return pscp(p);
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
 * this identifier will be in DECL state. It will display an error
 * message if the identifier is already in the global scope.
 */
void
pushscope(char *x)
{
	struct Id *id=xalloc(sizeof *id);

	id->x=x;
	id->st=DECL;
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

/* eins - Insert a binding in an environment. The environment
 * cell is allocated on the temporary heap.
 */
struct Env *
eins(struct Env *e, char *id, struct Term *ty)
{
	struct Env *pe=dkalloc(sizeof *pe);

	pe->l.s=id;
	pe->l.n=(struct L *)e;
	pe->t=ty;

	if (e) {
		pe->p=e->p;
		e->p=pe;
	} else
		pe->p=pe;
	return pe;
}

/* eget - Retreive a type from the environment, if the given
 * identifier does not have a matching type, 0 is returned.
 */
struct Term *
eget(struct Env *e, char *x)
{
	while (e) {
		if (e->l.s==x)
			return e->t;
		e=(struct Env *)e->l.n;
	}
	return 0;
}

/* eiter - Iterate a function on an environment.
 */
void
eiter(struct Env *e, void (*f)(char *, struct Term *, void *), void *p)
{
	struct Env *pe;

	if (!e)
		return;
	for (pe=e->p; pe!=e; pe=pe->p)
		f(pe->l.s, pe->t, p);
	f(pe->l.s, pe->t, p);
}

/* elen - Return the size of an environment.
 */
size_t
elen(struct Env *e)
{
	size_t s=0;

	while (e) {
		s++;
		e=(struct Env *)e->l.n;
	}
	return s;
}

/* escope - Check that an environment is well scoped.
 * If the environment is not properly scoped, 1 is returned,
 * otherwise, 0 is returned. If it succeeds, all name
 * appearing unbound will be qualified.
 */
int
escope(struct Env *e)
{
	while (e) {
		if (tscp(e->t, e->l.n))
			return 1;
		e=(struct Env *)e->l.n;
	}
	return 0;
}
