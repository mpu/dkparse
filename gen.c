#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dk.h"
#define emit printf

/* ------------- Pattern compilation. ------------- */

/* MAXPDPTH - The maximum depth of a pattern.
 */
#define MAXPDPTH 32

/* struct Pat - Patterns are constructors applied to
 * a list of other patterns.
 * The loc field is a 0 terminated array of integers
 * representing the path where the constructor is located
 * in the pattern.
 * As a convention, if ar is negative, the pattern is
 * a variable, if c is null, then it is a non-failing
 * pattern (a wildcard).
 */
struct Pat {
	char *c;
	int ar, *loc;
	struct Pat *ps;
};

/* struct PMat - A pattern matrix.
 */
struct PMat {
	struct Pat **m;
	int *rs;
	int r, c;
};

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
	assert(!rvp[i].p);
	rvp[i].p=duppath(dpth);
	return;
}

/* internal pat - Create a pattern corresponding to the
 * input term, variables in the re environment are seen
 * as pattern variables. The dpth argument is used to keep
 * track of the current depth while browsing the pattern.
 */
static void
pat(int dpth, struct Pat *p, struct Term *t)
{
	int i;

	if (dpth>=MAXPDPTH) {
		fprintf(stderr, "%s: Maximum pattern depth exceeded.\n"
		                "\tThe maximum is %d.\n", __func__, MAXPDPTH);
		exit(1); // FIXME
	}
	if (t->typ==Var && eget(re, t->uvar)) {
		vsetpath(dpth, t->uvar);
		p->ps=0;
		p->ar=-1;
		p->loc=0;
		p->c=t->uvar;
	} else {
		p->ar=napps(t, 0);
		p->ps=dkalloc(p->ar*sizeof *p->ps);
		for (i=p->ar-1; i>=0; i--, t=t->uapp.t1) {
			ploc[dpth]=i+1;
			pat(dpth+1, &p->ps[i], t->uapp.t2);
		}
		p->loc=duppath(dpth);
		assert(t->typ==Var);
		p->c=t->uvar;
	}
}

/* internal pmnew - Create a new pattern matrix corresponding
 * to a set of rewrite rules. It also fills the vpa array with
 * proper paths for each variable occuring in a pattern.
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
	m.rs=dkalloc(m.r*sizeof *m.rs);

	for (i=0; i<m.r; i++) {
		m.m[i]=dkalloc(m.c*sizeof *m.m[i]);
		m.rs[i]=i;
		rvp=vpa[i];
		re=rs->s[i].e;
		for (n=m.c-1, t=rs->s[i].l; n>=0; n--, t=t->uapp.t1) {
			ploc[0]=n+1;
			pat(1, &m.m[i][n], t->uapp.t2);
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
		n.rs=0;
		n.m=0;
		return n;
	}
	n.rs=dkalloc(n.r*sizeof *n.rs);
	for (k=i=0; i<n.r; k++)
		if (m.m[k][c].c==x || m.m[k][c].ar<0)
			n.rs[i++]=m.rs[k];
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
				n.m[i][c+j].loc=0;
				n.m[i][c+j].ps=0;
			}
			memcpy(n.m[i]+c+ar, ps+c+1, (n.c-c-ar)*spat);
			i++;
		}
	}
	return n;
}

/* internal pmdef - Create a default a pattern matrix.
 * The index given specifies the column to be specialized.
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
		n.rs=0;
		n.m=0;
		return n;
	}
	n.rs=dkalloc(n.r*sizeof *n.rs);
	for (k=i=0; i<n.r; k++)
		if (m.m[k][c].ar<0)
			n.rs[i++]=m.rs[k];
	if (n.c==0) {
		n.m=0;
		return n;
	}
	n.m=dkalloc(n.r*sizeof *n.m);

	for (k=i=0; i<n.r; k++)
		if (m.m[k][c].ar<0) {
			n.m[i]=dkalloc(n.c*spat);
			memcpy(n.m[i], m.m[k], c*spat);
			memcpy(n.m[i]+c, m.m[k]+c+1, (n.c-c)*spat);
			i++;
		}
	return n;
}

/* ------------- Code generation. ------------- */

/* This code generation section is splitted in four parts, the
 * first generate code to be added at the beginning of a
 * module the second compiles rule sets, the third, shorter,
 * compiles declarations, and the fourth provides utility
 * functions for the others, it mostly deals with the
 * compilation of terms to their dynamic and static
 * representation.
 */

/* enum NameKind - Specify the name kind that must be generated
 * for a variable, C means 'Code' and T means 'Term'. This is
 * used to call the gname function.
 */
enum NameKind { C, T };

static char *gname(enum NameKind, char *);
static void gterm(struct Term *);
static void gcode(struct Term *);

/* ------------- Module compiling. ------------- */

/* genmod - Generate the code to prepend to a compiled module,
 * it initializes the current module table.
 */
void
genmod(void)
{
	const char *m, *p, *q;

	q=m=mget();
	emit("--[[ Code for module %s. ]]\n", m);
	emit("local "); /* This line causes a 20% speedup. */
	while ((p=strchr(q, '.'))) {
		emit("%.*s = { }\n", (int)(p-m), m);
		q=p+1;
	}
	emit("%s = { }\n\n", m);
}

/* ------------- Rule set compiling. ------------- */

/* internal crs - This variable stores the rule set currently
 * being compiled.
 */
struct RSet *crs;

/* internal vpa esz - The vpa array stores variable, path
 * bindings for each branch of the pattern matching (each
 * rule of the rule set). The integer esz[i] is the length
 * of the array vpa[i] (the number of pattern variables for
 * the rule i).
 */
struct VPth **vpa;
int *esz;

/* internal gpath - Generate the expression to access
 * the object stored at the given path.
 */
static inline void
gpath(int *path)
{
	int i;

	emit("y%d", path[0]);
	for (i=1; path[i]; i++)
		emit(".args[%d]", path[i]);
}

/* internal gcond - Generate the condition of if statements
 * that guard an entry in the decision tree.
 */
static void
gcond(int *path, char *c)
{
	emit("if ");
	gpath(path);
	emit(".ck == ccon and ");
	gpath(path);
	emit(".ccon == \"%s\" then\n", c);
}

/* internal glocals - Generate the binding list local to the
 * rule i.
 */
static void
glocals(int i)
{
	int v;

	assert(i<crs->i);
	if (esz[i]==0)
		return;
	emit("local ");
	emit("%s", gname(C, vpa[i][0].x));
	for (v=1; v<esz[i]; v++)
		emit(", %s", gname(C, vpa[i][v].x));
	emit(" = ");
	gpath(vpa[i][0].p);
	for (v=1; v<esz[i]; v++) {
		emit(", ");
		gpath(vpa[i][v].p);
	}
	emit("\n");
}

/* internal grules - Compile a rule set following the pattern
 * matching compilation algorithm described in a paper by
 * Luc Maranget:
 *
 *   Compiling Pattern Matching to Good Decision Trees, ML'08.
 */
static void
grules(struct PMat pm)
{
	int i, c, sep;
	struct Pat *p;
	struct PMat m;

	if (pm.r==0) {
		emit("return { ck = ccon, ccon = \"%s\", args = { ", crs->x);
		for (sep=0, i=1; i<=crs->ar; i++, sep=1)
			emit(sep ? ", y%d" : "y%d", i);
		emit(" } }");
		return;
	}
	for (c=0; c<pm.c; c++)
		if (pm.m[0][c].ar>=0)
			break;
	if (c==pm.c) {
		glocals(pm.rs[0]);
		emit("return ");
		gcode(crs->s[pm.rs[0]].r);
		return;
	}

	p=&pm.m[0][c];
	gcond(p->loc, p->c);
	m=pmspec(pm, p->c, p->ar, c);
	grules(m);

	for (i=1; i<pm.r; i++) {
		p=&pm.m[i][c];
		if (p->ar<0)
			continue;
		emit("\nelse");
		gcond(p->loc, p->c);
		m=pmspec(pm, p->c, p->ar, c);
		grules(m);
	}

	emit("\nelse\n");
	m=pmdef(pm, c);
	grules(m);
	emit("\nend");
}

/* internal gchkenv - Generate code to type check one binding
 * of a rewrite rule's environment.
 */
static void
gchkenv(char *x, struct Term *t, void *unused)
{
	emit("chkbeg(\"%s\")\n", x);
	emit("chksort(");
	gterm(t);
	emit(")\n%s = { ck = ccon, ccon = \"%s\", args = { } }\n", gname(C, x), x);
	emit("%s = { tk = tbox, tbox = { ", gname(T, x));
	gcode(t);
	emit(", %s } }\n", gname(C, x));
	emit("chkend(\"%s\")\n", x);
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

/* genrules - Generate code to type check a rule set.
 */
void
genrules(struct RSet *rs)
{
	int i, v;
	struct VPth *ppa;
	struct PMat pm;

	crs=rs;
	vpa=dkalloc(rs->i*sizeof *vpa);
	esz=dkalloc(rs->i*sizeof *esz);
	for (i=0; i<rs->i; i++) {
		esz[i]=elen(rs->s[i].e);
		if (!esz[i]) {
			vpa[i]=0;
			continue;
		}
		ppa=vpa[i]=dkalloc(esz[i]*sizeof *vpa[i]);
		eiter(rs->s[i].e, fillvpa, &ppa);
	}

	if (rs->ar==0) {
		emit("--[[ Type checking the definition of %s. ]]\n", rs->x);
		emit("chkbeg(\"definition of %s\")\n", rs->x);
		emit("chk(");
		gterm(rs->s[0].r);
		emit(", %s.tbox[1])\n", gname(T, rs->x));
		emit("chkend(\"definition of %s\")\n", rs->x);
		emit("%s = ", gname(C, rs->x));
		gcode(rs->s[0].r);
		emit("\n\n");
		return;
	}
	emit("--[[ Type checking rules of %s. ]]\n", rs->x);
	emit("function check_rules()\nchkbeg(\"rules of %s\")\n", rs->x);
	for (i=0; i<rs->i; i++) {
		emit("chkbeg(\"rule %d\")\n", i+1);
		eiter(rs->s[i].e, gchkenv, 0);
		emit("do\nlocal ty = synth(0, ");
		gterm(rs->s[i].l);
		emit(")\nchk(");
		gterm(rs->s[i].r);
		emit(", ty)\nend\nchkend(\"rule %d\")\n", i+1);
	}
	emit("chkend(\"rules of %s\")\nend\ncheck_rules()\n", rs->x);
	emit("--[[ Compiling rules of %s. ]]\n", rs->x);
	emit("%s = { ck = clam, arity = %d, args = { }, clam =\n", gname(C, rs->x), rs->ar);
	emit("function (y1");
	for (i=2; i<=rs->ar; i++)
		emit(", y%d", i);
	emit(")\n");
	pm=pmnew(rs, vpa);
	grules(pm);
	emit("\nend }\n\n");
}

/* ------------- Declaration compiling. ------------- */

/* gendecl - Generate code to type check a declaration.
 */
void
gendecl(char *x, struct Term *t)
{
	emit("--[[ Type checking %s. ]]\n", x);
	gchkenv(x, t, 0);
	emit("\n");
}

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
gname(enum NameKind nt, char *x)
{
	static char s[MAXID], *p;

	for (p=s; *x; x++, p++) {
		if (p-s>=MAXID-4) {
			fprintf(stderr, "%s: Maximum identifier length exceeded.\n"
			                "\tThe maximum is %d.\n", __func__, MAXID);
			exit(1);
		}
		if (*x=='x' || *x=='\'') {
			*p++='x';
			*p=*x=='\''?'q':*x;
			continue;
		}
		*p=*x;
	}
	*p++='_';
	*p++=nt==C?'c':'t';
	*p=0;
	return s;
}

/* internal gcode - Emit an expression representing the dynamic
 * translation of a term in the lambda-Pi calculus.
 */
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
		emit("{ ck = clam, arity = %d, args = { }, clam = function (", s);
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

/* internal gterm - Emit an expression representing the static
 * translation of a term in the lambda-Pi calculus.
 */
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
		emit("%s) return ", gname(C, t->ulam.x));
		gterm(t->ulam.t);
		emit(" end } }");
		break;
	case Pi:
		emit("{ tk = tpi; tpi = { ");
		if (t->upi.ty->typ==Var)
			emit("%s", gname(C, t->upi.ty->uvar));
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
