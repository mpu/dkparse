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
pushrule(struct Env *e, struct Pat *l, struct Term *r)
{
	if (rs.i>=MAXRULES) {
		fprintf(stderr, "%s: Too many rules accumulated.\n"
		                "\tThe limit is %d.\n", __func__, MAXRULES);
		return 1;
	}
	rs.s[rs.i].vpa=0;
	rs.s[rs.i].elen=elen(e);
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
	pdone();
	rs.i=0;
	rs.x=0;
}

/* ------------- Rule checking. ------------- */

/* internal rchk - Checks that the current rule set is valid.
 * If it is not, 1 is returned, 0 is returned in case of success.
 */
static int
rchk(void)
{
#define fail(...) do { fprintf(stderr, __VA_ARGS__); return 1; } while (0)
	int r, da, pa;

	assert(rs.i>0);
	rs.x=mqual(rs.s[0].l->c);
	da=rs.s[0].l->nd;
	pa=rs.s[0].l->np;

	if (da+pa==0 && rs.i!=1)
		fail("%s: A constant must not have more than"
		     " one rewrite rule.\n", __func__);

	for (r=0; r<rs.i; r++) {
		if (eget(rs.s[r].e, rs.x))
			fail("%s: Head symbol must not be in environment.\n"
			     ,__func__);
		if (da!=rs.s[r].l->nd || pa!=rs.s[r].l->np)
			fail("%s: All rules must have the same arity.\n"
			     ,__func__);
		if (escope(rs.s[r].e))
			fail("%s: Environment is not properly scoped.\n"
			     ,__func__);
		if (pscope(rs.s[r].l, rs.s[r].e) || tscope(rs.s[r].r, rs.s[r].e))
			return 1;
		if (pchk(&rs.s[r]))
			return 1;
	}
	return 0;
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
	/* genrules(&rs); */
	flushrules();
	dkfree();
}
