#include <stdio.h>
#include <stdlib.h>
#include "dk.h"

/* ------------- Parsing arrays. ------------- */

/* PALEN - Default size of parsing arrays.
 */
#define PALEN 8

/* struct PArr - Parsing arrays are used to build
 * pattern structures while parsing. Note that they
 * make use of the C99 flexible array member feature.
 */
struct PArr {
	size_t i, sz;
	void *p[];
};

/* internal panew - Return a new dynamic parsing array.
 */
static struct PArr *
panew(void)
{
	struct PArr *pa;
	pa=xalloc(sizeof *pa + PALEN*sizeof *pa->p);
	pa->i=0;
	pa->sz=PALEN;
	return pa;
}

/* pains - Insert a new element in a parsing array, if
 * the array is too small, it is resized. If the input
 * pointer pa is null, a new array is allocated.
 */
struct PArr *
pains(struct PArr *pa, void *p)
{
	if (!pa)
		pa=panew();
	if (pa->i>=pa->sz) {
		pa->sz*=2;
		pa=xrealloc(pa, sizeof *pa + pa->sz*sizeof *pa->p);
	}
	pa->p[pa->i++]=p;
	return pa;
}

/* ------------- Pattern construction. ------------- */

/* mkpat - Create a pattern node, the first parsing array
 * stores parsed terms (dot patterns), the second stores
 * parsed patterns. The string argument in c is the
 * applied constructor/variable.
 */
struct Pat *
mkpat(char *c, struct PArr *pads, struct PArr *paps)
{
	size_t s;
	struct Pat *p;

	p=dkalloc(sizeof *p);
	p->c=c;
	p->nd=p->np=0;
	p->loc=0;
	p->ds=0;
	p->ps=0;
	if (pads) {
		p->nd=pads->i;
		p->ds=dkalloc(p->nd*sizeof *p->ds);
		for (s=0; s<p->nd; s++)
			p->ds[s]=(struct Term *)pads->p[s];
		free(pads);
	}
	if (paps) {
		p->np=paps->i;
		p->ps=dkalloc(p->np*sizeof *p->ps);
		for (s=0; s<p->np; s++)
			p->ps[s]=(struct Pat *)paps->p[s];
		free(paps);
	}
	return p;
}

/* ------------- Array functions. ------------- */

/* ALEN - Constant used to initialize the size of arrays
 * ar and vr defined below.
 */
#define ALEN 1024

/* struct IdN - This structure associates a symbol with
 * a number, it is used to check well formation of
 * rewriting patterns.
 */
struct IdN {
	char *x;
	int n;
};

/* internal dar par vr - These three arrays are used to
 * associate constructors with arities (dar and par) and
 * to associate variables to their number of uses in
 * patterns (vr).
 */
static struct Arr {
	struct IdN t[ALEN];
	size_t i;
} dar, par, vr;

/* internal tset - Change the integer associated to an
 * identifier in the arrays dar, par or vr. The old value
 * of the integer is returned, if the identifier was not
 * in the array, -1 is returned.
 */
static inline int
tset(struct Arr *a, struct IdN id)
{
	int r;
	size_t i;

	for (i=0; i<a->i; i++)
		if (a->t[i].x==id.x) {
			r=a->t[i].n;
			a->t[i].n=id.n;
			return r;
		}
	if (a->i>=ALEN) {
		fprintf(stderr, "%s: Maximum array size reached.\n"
		                "\tThe maximum is %d.\n", __func__, ALEN);
		exit(1); // FIXME
	}
	a->t[a->i++]=id;
	return -1;
}

/* internal tget - Retreive the integer associated to
 * an identifier in an array. If the identifier is not
 * bound, then -1 is returned.
 */
static inline int
tget(struct Arr *a, char *x)
{
	size_t i;

	for (i=0; i<a->i; i++)
		if (a->t[i].x==x)
			return a->t[i].n;
	return -1;
}

/* ------------- Pattern checking. ------------- */

/* MAXPDPTH - The maximum depth of a pattern.
 */
#define MAXPDPTH 32

/* internal rvp - The associative array used to store
 * the mapping from pattern variables of the current
 * rule to paths.
 */
static struct VPth *rvp;

/* internal ploc - This stores the current path during the
 * checking of patterns.
 */
static int ploc[MAXPDPTH];

/* internal duppath - Duplicate the current path and return it.
 * The returned path will be a 0 terminated array of dpth+1
 * elements.
 */
static int *
duppath(int dpth)
{
	int *p=dkalloc((dpth+1)*sizeof *p);
	int i;

	for (i=0; i<dpth; i++)
		p[i]=ploc[i];
	p[i]=0;
	return p;
}

/* internal vsetpath - Bind one variable to the current path.
 */
static void
vsetpath(int dpth, char *x)
{
	int i;

	for (i=0; rvp[i].x!=x; i++)
		;
	rvp[i].p=duppath(dpth);
	return;
}

/* internal chkpat - Check that a nested pattern respects
 * well formation rules, it also fills location information
 * (in the pattern and in the rvp array for variables).
 */
static int
chkpat(int dpth, struct Pat *p)
{
	int a;
	struct IdN id;

	if (dpth>=MAXPDPTH) {
		fprintf(stderr, "%s: Maximum pattern depth exceeded.\n"
		                "\tThe maximum is %d.\n", __func__, MAXPDPTH);
		exit(1); // FIXME
	
	}
	id.x=p->c;
	if (p->nd<0) { /* We recognized a variable. */
		if (p->nd+p->np==0) {
			id.n=tget(&vr, id.x)+1;
			tset(&vr, id);
			vsetpath(dpth, id.x);
			return 0;
		}
		fprintf(stderr, "%s: Pattern variable %s cannot be applied.\n"
		              , __func__, id.x);
		return 1;
	}
	for (a=0; a<p->np; a++) {
		ploc[dpth]=a+p->nd+1;
		if (chkpat(dpth+1, p->ps[a]))
			return 1;
	}
	id.n=p->nd;
	a=tset(&dar, id);
	if (a>=0 && a!=id.n) {
		fprintf(stderr, "%s: Constructor %s must not have several"
		                " dot arities (%d and %d).\n"
		              , __func__, id.x, a, id.n);
		return 1;
	}
	id.n=p->np;
	a=tset(&par, id);
	if (a>=0 && a!=id.n) {
		fprintf(stderr, "%s: Constructor %s must not have several"
		                " regular arities (%d and %d).\n"
		              , __func__, id.x, a, id.n);
		return 1;
	}
	p->loc=duppath(dpth);
	return 0;
}

/* internal chkenv - Checks that a member of the environment
 * is in the array vr once. If not, it sets the integer at
 * *(int *)peerr to 1.
 */
static void
chkenv(char *x, struct Term *t, void *peerr)
{
	int a;

	a=tget(&vr, x);
	if (a<0) {
		fprintf(stderr, "%s: Variable %s does not appear in pattern.\n"
		              , __func__, x);
		*(int *)peerr=1;
	}
	else if (a>0) {
		fprintf(stderr, "%s: Variable %s appears %d times in pattern.\n"
		              , __func__, x, a+1);
		*(int *)peerr=1;
	}
}

/* internal fillvpa - This is a helper function called by
 * eiter to initialize an array of struct VPth objects.
 * The x fields are set to elements of the environment and
 * the p fields are set to 0.
 */
static void
fillvpa(char *x, struct Term *t, void *ppa)
{
	struct VPth **pp=ppa;

	(*pp)->x=x;
	(*pp)->p=0;
	(*pp)++;
}

/* pchk - Check that a given rule pattern respect well
 * formation rules with respect to the rule environment.
 * It also fills location information for contructors and
 * variables by filling loc fields in r->l, and by
 * allocating and filling the r->vpa array.
 * If the pattern is not valid, 1 is returned, 0
 * otherwise.
 */
int
pchk(struct Rule *r)
{
	int i, eerr;
	struct VPth *vp;

	if (r->elen) {
		rvp=vp=r->vpa=dkalloc(r->elen*sizeof *r->vpa);
		eiter(r->e, fillvpa, &vp);
	} else
		rvp=r->vpa=0;
	vr.i=0;
	for (i=0; i<r->l->np; i++) {
		ploc[0]=i+1;
		if (chkpat(1, r->l->ps[i]))
			return 1;
	}
	eerr=0;
	eiter(r->e, chkenv, &eerr); /* Check linearity. */
	return eerr;
}

/* pdone - This function reset the internal state of the
 * pattern checker, it must be called at the end of the
 * checking of a rule set. Namely, the internal state
 * is the mappings from constructors to arities (dar and
 * par).
 */
void
pdone(void)
{
	dar.i=par.i=0;
}
