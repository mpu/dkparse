#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dk.h"

/* internal rs - This stack is used by the parser to
 * accumulate related rewrite rules (rules concerning a
 * given identifier). The ar field stores the arity of
 * the symbol defined by this rule set.
 */
static struct RSet rs;

/* ------------- Rule stack manipulations. ------------- */

/* pushrule - Add a rule to the current rule set, if not enough
 * memory is available, 1 is returned, otherwise, 0 is returned.
 */
int
pushrule(struct Env *e, struct Term *l, struct Term *r)
{
	if (rs.i>=MAXRULES) {
		fprintf(stderr, "%s: Too many rules accumulated.\n"
		                "\tThe limit is %d.\n", __func__, MAXRULES);
		return 1;
	}
	rs.s[rs.i].e=e;
	rs.s[rs.i].l=l;
	rs.s[rs.i].r=r;
	rs.i++;
	return 0;
}

/* internal flushrules - Flushes the current rule set, this is
 * used when the whole rule set has been fully processed.
 */
static void
flushrules(void)
{
	rs.i=0;
	rs.ar=-1;
	rs.x=0;
}

/* ------------- Rule checking. ------------- */

/* ALEN - Constant used to initialize the size of arrays
 * ar and vr defined below.
 */
#define ALEN 1024

/* struct IdN - This structure associates a symbol with a number,
 * it is used to check well formation of rewrite rules.
 */
struct IdN {
	char *x;
	int n;
};

/* internal ar vr - These two arrays are used to associate constructors
 * with arities (ar) and to associate variables to their number of uses
 * in patterns (in vr).
 */
static struct Arr {
	struct IdN t[ALEN];
	size_t i;
} ar, vr;

/* internal tset - Change the integer associated to an identifier
 * in the arrays ar or vr. The old value of the integer is returned,
 * if the identifier was not in the array, -1 is returned.
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

/* internal tget - Retreive the integer associated to an identifier
 * in an array. If the identifier is not bound, then -1 is returned.
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

/* internal chkrule - Checks that a given term in the left hand
 * side of a rule is valid. The environment provided is the
 * environment of the checked lhs. The number of uses of pattern
 * variables is updated in the vr array.
 */
static int
chklhs(struct Env *e, struct Term *t)
{
	int a;
	struct IdN id;

	for (id.n=0; t->typ==App; t=t->uapp.t1, id.n++) {
		if (chklhs(e, t->uapp.t2))
			return 1;
	}
	if (t->typ!=Var) {
		fprintf(stderr, "%s: Only constructors can be applied"
		                " in patterns.\n"
		              , __func__);
		return 1;
	}
	id.x=t->uvar;
	if (eget(e, id.x)) {
		if (id.n==0) {
			id.n=tget(&vr, id.x)+1;
			tset(&vr, id);
			return 0;
		}
		fprintf(stderr, "%s: Pattern variable %s cannot be applied.\n"
		              , __func__, id.x);
		return 1;
	}
	a=tset(&ar, id);
	if (a>=0 && a!=id.n) {
		fprintf(stderr, "%s: Constructor %s must not have several"
		                " arities (%d and %d).\n"
		              , __func__, id.x, a, id.n);
		return 1;
	}
	return 0;
}

/* internal chkenv - Checks that a member of the environment is in
 * the array vr. If not, it sets the integer at *(int *)peerr to 1.
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

/* internal rchk - Checks that the current rule set is valid.
 * It it is not, 1 is returned, 0 is returned in case of success.
 */
static int
rchk(void)
{
#define fail(...) do { fprintf(stderr, __VA_ARGS__); goto err; } while (0)
	int r, a, eerr;
	struct Term *t;

	assert(rs.i>0);
	ar.i=0;

	rs.ar=napps(rs.s[0].l, &t);
	if (t->typ!=Var)
		fail("%s: Head of the rule must be a constant.\n", __func__);
	rs.x=mqual(t->uvar);

	if (rs.ar==0 && rs.i!=1)
		fail("%s: A constant must not have more than"
		     " one rewrite rule.\n", __func__);

	for (r=0; r<rs.i; r++) {
		if (escope(rs.s[r].e))
			fail("%s: Environment is not properly scoped.\n"
			     ,__func__);
		if (scope(rs.s[r].l, rs.s[r].e) || scope(rs.s[r].r, rs.s[r].e))
			goto err;
		if (eget(rs.s[r].e, rs.x))
			fail("%s: Head symbol must not be in environment.\n"
			     ,__func__);

		for (t=rs.s[r].l, vr.i=a=0; t->typ==App; t=t->uapp.t1, a++) {
			if (chklhs(rs.s[r].e, t->uapp.t2))
				goto err;
		}
		eerr=0;
		eiter(rs.s[r].e, chkenv, &eerr);
		if (eerr)
			goto err;

		if (t->typ!=Var || t->uvar!=rs.x)
			fail("%s: All rewrite rules must have the same"
			     " head constant.\n", __func__);
		if (a!=rs.ar)
			fail("%s: All rules must have the same arity.\n"
			     ,__func__);
	}
	return 0;
err:
	return 1;
#undef fail
}

/* dorules - Main processing of a rule set.
 */
void
dorules(void)
{
	if (rchk()) {
		fprintf(stderr, "%s: Checking for rules of %s failed.\n"
		              , __func__, rs.x?rs.x:"???");
		exit(1); // FIXME
	}
	if (chscope(rs.x, DEF)==DEF) {
		fprintf(stderr, "%s: Identifer %s must not be defined twice.\n"
		              , __func__, rs.x);
		exit(1); // FIXME
	}
	genrules(&rs);
	flushrules();
	dkfree();
}
