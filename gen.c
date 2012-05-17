#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dk.h"
#define emit printf

/* ------------- Pattern compilation. ------------- */

/* struct Pat - Patterns are constructors applied to
 * a list of other patterns.
 * As a convention, if ar is negative, the pattern is
 * a variable, if c is null, then it is a non-failing
 * pattern (a wildcard).
 */
struct Pat {
	char *c;
	int ar;
	struct Pat *ps;
};

/* struct PMat - A pattern matrix.
 */
struct PMat {
	struct Pat **m;
	struct Term **rhs;
	int r, c;
};

/* MAXPDPTH - The maximum depth of a pattern.
 */
#define MAXPDPTH 32

/* struct VPth - Associate a pattern variable to its path.
 * The path must be 0 terminated.
 */
struct VPth {
	char *x;
	int *p;
};

/* internal ploc - This stores the current path during the
 * construction of the initial pattern matrix.
 */
static int ploc[MAXPDPTH];

/* internal re rvp - These two variables store the environment
 * and the bindings from pattern variables to paths of the
 * rule which is currently compiled to a pattern matrix.
 */
static struct Env *re;
static struct VPth *rvp;

/* internal vsetpath - Bind one variable to the current path.
 */
static void
vsetpath(int dpth, char *x)
{
	int *p=dkalloc((dpth+1)*sizeof *p);
	int i;

	for (i=0; i<dpth; i++)
		p[i]=ploc[i];
	p[i]=0;
	for (i=0; rvp[i].x!=x; i++)
		;
	assert(!rvp[i].p);
	rvp[i].p=p;
	return;
}

/* internal pat - Create a pattern corresponding to the
 * input term, variables in the provided environment are
 * seen as pattern variables. The dpth argument is used
 * to keep track of the current depth while browsing the
 * pattern.
 */
static void
pat(int dpth, struct Pat *p, struct Term *t)
{
	int i;

	if (dpth>=MAXPDPTH-1) {
		fprintf(stderr, "%s: Maximum pattern depth exceeded.\n"
		                "\tThe maximum is %d.\n", __func__, MAXPDPTH);
		exit(1); // FIXME
	}
	if (t->typ==Var && eget(re, t->uvar)) {
		vsetpath(dpth, t->uvar);
		p->ps=0;
		p->ar=-1;
		p->c=t->uvar;
	} else {
		p->ar=napps(t, 0);
		p->ps=dkalloc(p->ar*sizeof *p->ps);
		for (i=p->ar-1; i>=0; i--, t=t->uapp.t1) {
			ploc[dpth+1]=i;
			pat(dpth+1, &p->ps[i], t->uapp.t2);
		}
		assert(t->typ==Var);
		p->c=t->uvar;
	}
}

/* internal pmnew - Create a new pattern matrix corresponding
 * to a set of rewrite rules. It also fills the vpa array with
 * proper paths for each variable occuring in the pattern.
 */
static struct PMat
pmnew(struct RSet *rs, struct VPth *vpa[])
{
	int i, n;
	struct PMat m;
	struct Term *t;

	assert(rs->i>0 && rs->ar>0);
	m.r=rs->i;
	m.c=rs->ar;
	m.m=dkalloc(m.r*sizeof *m.m);
	m.rhs=dkalloc(m.r*sizeof *m.rhs);

	for (i=0; i<m.r; i++) {
		m.m[i]=dkalloc(m.c*sizeof *m.m[i]);
		m.rhs[i]=rs->s[i].r;
		rvp=vpa[i];
		re=rs->s[i].e;
		for (n=m.c-1, t=rs->s[i].l; n>=0; n--, t=t->uapp.t1) {
			ploc[0]=n;
			pat(0, &m.m[i][n], t->uapp.t2);
		}
	}
	return m;
}

/* ------------- Pattern matrices operations. ------------- */

/* internal pmspec - Create a pattern matrix by specializing
 * the column of another pattern matrix for a given constructor
 * of a given arity.
 * The index given specifies the column to be specialized.
 */
static struct PMat
pmspec(struct PMat m, char *x, int ar, int c)
{
	const size_t spat=sizeof (struct Pat);
	int i, j, k;
	struct PMat n;

	assert(c>=0 && c<m.c && m.r>0);
	for (n.r=i=0; i<m.r; i++) {
		if (m.m[i][c].c==x || m.m[i][c].ar<0)
			n.r++;
	}
	n.c=m.c-1+ar;
	if (n.r==0) {
		n.rhs=0;
		n.m=0;
		return n;
	}
	n.rhs=dkalloc(n.r*sizeof *n.m);
	for (k=i=0; i<n.r; k++)
		if (m.m[k][c].c==x || m.m[i][c].ar<0)
			n.rhs[i++]=m.rhs[k];
	if (n.c==0) {
		n.m=0;
		return n;
	}
	n.m=dkalloc(n.r*sizeof *n.m);

	for (k=i=0; i<n.r; k++) {
		struct Pat *ps=m.m[k];
		if (ps[c].c==x) {
			n.m[i]=dkalloc(n.c*spat);
			memcpy(n.m[i], ps, c*spat);
			memcpy(n.m[i]+c, ps[c].ps, ar*spat);
			memcpy(n.m[i]+c+ar, ps+c+1, (n.c-c-ar)*spat);
			i++;
		}
		else if (ps[c].ar<0) {
			n.m[i]=dkalloc(n.c*spat);
			memcpy(n.m[i], ps, c*spat);
			for (j=0; j<ar; j++) {
				n.m[i][c+j].c=0;
				n.m[i][c+j].ar=-1;
				n.m[i][c+j].ps=0;
			}
			memcpy(n.m[i]+c+ar, ps+c+1, (n.c-c-ar)*spat);
			i++;
		}
	}
	return n;
}

/* internal pmdef - Default a pattern matrix. This occurs when
 * we generate code for a wildcard branch. This is used in comprs
 * later on.
 */
static struct PMat
pmdef(struct PMat m, int c)
{
	const size_t spat=sizeof (struct Pat);
	int i, k;
	struct PMat n;

	assert(c<m.c);
	for (n.r=0, i=0; i<m.r; i++)
		if (m.m[i][c].ar<0)
			n.r++;
	n.c=m.c-1;
	if (n.r==0) {
		n.rhs=0;
		n.m=0;
		return n;
	}
	n.rhs=dkalloc(n.r*sizeof *n.m);
	for (k=i=0; i<n.r; k++)
		if (m.m[i][c].ar<0)
			n.rhs[i++]=m.rhs[k];
	if (n.c==0) {
		n.m=0;
		return n;
	}
	n.m=dkalloc(n.r*sizeof *n.m);

	for (k=i=0; i<n.r; k++)
		if (m.m[k][c].ar<0) {
			n.m[i]=dkalloc(n.c*spat);
			memcpy(n.m[i], m.m[k]+1, n.c*spat);
			i++;
		}
	return n;
}

/* ------------- Code generation. ------------- */

/* This code generation section is splitted in three parts,
 * the first compiles rule sets, the second, shorter,
 * compiles declarations, and the third provides utility
 * functions for the two first parts, it mostly deals with
 * the compilation of terms to their dynamic and static
 * representation.
 */

/* enum NameType - Specify the name type that must be generated
 * for a variable, C means 'Code' and T means 'Term'. This is
 * used to call the gname function.
 */
enum NameType { C, T };

static char *gname(enum NameType, char *);
static void gterm(struct Term *);
static void gcode(struct Term *);

/* ------------- Rule set compiling. ------------- */

/* internal comprs - Compile a rule set following a pattern
 * matching algorithm described in a paper by Luc Maranget:
 *
 *   Compiling Pattern Matching to Good Decision Trees, ML'08.
 */
static void
comprs(struct PMat pm)
{
	int i, c;
	struct Pat *p;
	struct PMat m;

	if (pm.r==0) {
		emit("--[[ FIXME ]]");
		return;
	}
	for (c=0; c<pm.c; c++)
		if (pm.m[0][c].ar>=0)
			break;
	if (c==pm.c) {
		emit("return ");
		gcode(pm.rhs[0]);
		return;
	}

	p=&pm.m[0][c];
	emit("if %s then\n", p->c);
	m=pmspec(pm, p->c, p->ar, c);
	comprs(m);

	for (i=1; i<pm.r; i++) {
		p=&pm.m[i][c];
		if (p->ar>=0) {
			emit("\nelseif %s then\n", p->c);
			m=pmspec(pm, p->c, p->ar, c);
			comprs(m);
		}
	}

	emit("\nelse\n");
	m=pmdef(pm, c);
	comprs(m);
	emit("\nend\n");
}

/* internal fillvpa - This function will fill the struct VPth object
 * designated by *ppa with the given environment. The path field
 * is initialized to null. Then it increments *ppa.
 */
static void
fillvpa(char *x, struct Term *t, void *ppa)
{
	struct VPth **pp=ppa;

	(*pp)->x=x;
	(*pp)->p=0;
	(*pp)++;
}

/* genrules - Generate code associated to a rule set.
 */
void
genrules(struct RSet *rs)
{
	int i;
	struct VPth *ppa, **vpa=dkalloc(rs->i*sizeof *vpa);
	int *esz=dkalloc(rs->i*sizeof *esz);
	struct PMat pm;

	for (i=0; i<rs->i; i++) {
		esz[i]=elen(rs->s[i].e);
		if (!esz[i]) {
			vpa[i]=0;
			continue;
		}
		ppa=vpa[i]=dkalloc(elen(rs->s[i].e)*sizeof *vpa[i]);
		eiter(rs->s[i].e, fillvpa, &ppa);
	}

	if (rs->ar>0) {
		emit("--[[ Compiling rules for %s. ]]\n", rs->x);
		pm=pmnew(rs, vpa);
		comprs(pm);
		emit("\n");
	}
	return;
}

/* ------------- Declaration compiling. ------------- */

/* ------------- Term compiling. ------------- */

/* MAXSDPTH - The maximum stack depth used in functions below,
 * these stacks are used to recurse on terms.
 */
#define MAXSDPTH 128

/* MAXID - Maximum size of an identifier in the generated code.
 */
#define MAXID 128

/* internal gname - Generate the variable name to be emitted to
 * access a variable. This name can be of two kinds, either a
 * 'code' name or a 'term' name.
 * Warning, this returns a static buffer.
 */
static inline char *
gname(enum NameType nt, char *x)
{
	static char s[MAXID];

	switch (nt) {
	case C:
		snprintf(s, MAXID, "%s_c", x);
		break;
	case T:
		snprintf(s, MAXID, "%s_t", x);
		break;
	}
	return s;
}

static void
gcode(struct Term *t)
{
	char **lams;
	int i, s;

/* tail: */
	switch (t->typ) {
	case Var:
		emit("%s", gname(C, t->uvar));
		break;
	case Lam:
		lams=xalloc(MAXSDPTH*sizeof *lams);
		for (s=0; t->typ==Lam; s++) {
			if (s>=MAXSDPTH) {
				fprintf(stderr, "%s: Maximum recursion depth exceeded.\n"
				                "\tThe maximum is %d.\n", __func__, MAXSDPTH);
				exit(1); // FIXME
			}
			lams[s]=t->ulam.x;
			t=t->ulam.t;
		}
		emit("{ ck = clam, arity = %d, args = {}, clam = function (", s);
		for (i=0; i<s-1; i++)
			emit("%s, ", gname(C, lams[i]));
		emit("%s) return ", gname(C, lams[i]));
		free(lams);
		gcode(t);
		emit(" end }");
		break;
	case Pi:
		emit("{ ck = cpi, cpi = { ");
		gcode(t->upi.ty);
		emit(", function (%s) return ",
		     t->upi.x ? gname(C, t->upi.x) : "dkhole");
		gcode(t->upi.t);
		emit(" end } }");
		break;
	case App:
		emit("ap(");
		gcode(t->uapp.t1);
		emit(", ");
		gcode(t->uapp.t2);
		emit(")");
		break;
	case Type:
		emit("{ ck = ctype }");
		break;
	}
}

static void
gterm(struct Term *t)
{
	char *x;
/* tail: */
	switch (t->typ) {
	case Var:
		emit("%s", gname(T, t->uvar));
		break;
	case Lam:
		emit("{ tk = tlam, tlam = { nil, ");
		emit("function (%s, ", gname(T, t->ulam.x));
		emit(", %s) return ", gname(C, t->ulam.x));
		gterm(t->ulam.t);
		emit(" end } }");
		break;
	case Pi:
		emit("{ tk = tpi; tpi = { ");
		if (t->upi.ty->typ==Var)
			emit(gname(C, t->upi.ty->uvar));
		else {
			emit("chkabs(");
			gterm(t->upi.ty);
			emit(", ");
			gcode(t->upi.ty);
			emit(")");
		}
		x = t->upi.x ? t->upi.x : "dkhole";
		emit(", function (%s, ", gname(T, x));
		emit("%s) return ", gname(C, x));
		gterm(t->upi.t);
		emit(" end } }");
		break;
	case App:
		emit("{ tk = tapp, tapp = { ");
		gterm(t->uapp.t1);
		emit(", ");
		gterm(t->uapp.t2);
		emit(", ");
		gcode(t->uapp.t2);
		emit(" } }");
		break;
	case Type:
		emit("{ tk = ttype }");
		break;
	}
}
