#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dk.h"
#include "lib/avl.h"
#define MAXRULES 32

/* internal rs - This stack is used by the parser to
 * accumulate related rewrite rules (rules concerning a
 * given identifier). The ar field stores the arity of
 * the symbol defined by this rule set.
 */
static struct {
	struct Rule *s[MAXRULES];
	int i, ar;
	char *x;
} rs;

/* ------------- Rule stack manipulations. ------------- */

/* pushrule - Add a rule to the current rule set, if not enough
 * memory is available, 1 is returned, otherwise, 0 is returned.
 */
int
pushrule(struct Rule *r)
{
	if (rs.i>=MAXRULES) {
		fprintf(stderr, "%s: Too many rules accumulated.\n"
		                "\tThe limit is %d.\n", __func__, MAXRULES);
		return 1;
	}
	rs.s[rs.i++]=r;
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

/* struct IdA - This structure associates a symbol with its arity,
 * it is used in balanced trees to check well formation of rewrite
 * rules.
 */
struct IdA {
	char *x;
	int ar;
};

/* internal idacmp - Compare to struct IdA, this is used by the
 * balanced tree library.
 */
static int
idacmp(void *a, void *b)
{
	return strcmp(((struct IdA *)a)->x, ((struct IdA *)b)->x);
}

/* internal chkrule - Checks that a given term in the left hand
 * side of a rule is valid. The environment provided is the
 * environment of the checked lhs. The balanced tree provided
 * contains mappings from constructors to arities.
 */
static int
chklhs(struct Tree *ar, struct Env *e, struct Term *t)
{
	struct IdA id, *pid;

	for (id.ar=0; t->typ==App; t=t->uapp.t1, id.ar++) {
		if (chklhs(ar, e, t->uapp.t2))
			return 1;
	}
	if (t->typ!=Var) {
		fprintf(stderr, "%s: Only variables can be applied"
		                " in patterns.\n"
		              , __func__);
		return 1;
	}
	id.x=t->uvar;
	if (eget(e, id.x)) {
		if (id.ar==0)
			return 0;
		fprintf(stderr, "%s: Pattern variable %s cannot be applied.\n"
		              , __func__, id.x);
		return 1;
	}
	pid=avlget(&id, ar);
	if (pid) {
		if (pid->ar!=id.ar) {
			fprintf(stderr, "%s: Constructor %s must have constant"
			                " arity (%d) in patterns.\n"
			              , __func__, id.x, id.ar);
			return 1;
		}
	} else {
		pid=xalloc(sizeof *pid);
		memcpy(pid, &id, sizeof id);
		avlins(pid, ar);
	}
	return 0;
}

/* internal rchk - Checks that the current rule set is valid.
 * It it is not, 1 is returned, 0 is returned in case of success.
 */
static int
rchk(void)
{
#define fail(...) do { fprintf(stderr, __VA_ARGS__); goto err; } while (0)
	int r, a;
	struct Tree *ar;
	struct Term *t;

	assert(rs.i>0);
	ar=avlnew(idacmp, free);
	rs.ar=napps(rs.s[0]->l, &t);
	if (t->typ!=Var)
		fail("%s: Head of the rule must be a constant.\n", __func__);
	rs.x=t->uvar;

	if (rs.ar==0 && rs.i!=1)
		fail("%s: A constant must not have more than"
		     " one rewrite rule.\n"
		     ,__func__);

	for (r=0; r<rs.i; r++) {
		if (escope(rs.s[r]->e))
			fail("%s: Environment is not properly scoped.\n"
			     ,__func__);
		if (eget(rs.s[r]->e, rs.x))
			fail("%s: Head symbol must not be in environment.\n"
			     ,__func__);

		for (t=rs.s[r]->l, a=0; t->typ==App; t=t->uapp.t1, a++) {
			if (chklhs(ar, rs.s[r]->e, t->uapp.t2))
				goto err;
		}
		if (t->typ!=Var || t->uvar!=rs.x)
			fail("%s: All rewrite rules must have the same"
			     " head constant.\n"
			     ,__func__);
		if (a!=rs.ar)
			fail("%s: All rules must have the same arity.\n"
			     ,__func__);
		if (scope(rs.s[r]->l, rs.s[r]->e) || scope(rs.s[r]->r, rs.s[r]->e))
			goto err;
	}
	avlfree(ar);
	return 0;
err:
	avlfree(ar);
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
	flushrules();
	dkfree();
}
