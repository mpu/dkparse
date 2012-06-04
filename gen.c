#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dk.h"
#define emit printf

/* ------------- Pattern matrices. ------------- */

/* struct PMat - A pattern matrix.
*/
struct PMat {
	struct Pat ***m;
	int *rs;
	int r, c;
};

/* internal pmnew - Create a new pattern matrix corresponding
 * to a set of rewrite rules. It simply needs to copy patterns
 * from the left hand side of rules in an array.
 */
static struct PMat
pmnew(struct RSet *rs)
{
	int r;
	struct PMat m;

	assert(rs->i>0 && rs->s[0].l->np>0);
	m.r=rs->i;
	m.c=rs->s[0].l->np;
	m.m=dkalloc(m.r*sizeof *m.m);
	m.rs=dkalloc(m.r*sizeof *m.rs);

	for (r=0; r<m.r; r++) {
		m.m[r]=dkalloc(m.c*sizeof *m.m[r]);
		m.rs[r]=r;
		memcpy(m.m[r], rs->s[r].l->ps, m.c*sizeof *m.m[r]);
	}
	return m;
}

/* internal pmspec - Create a pattern matrix by specializing
 * the column of another pattern matrix for a given constructor
 * of a given arity.
 * The index given specifies the column to be specialized.
 */
static struct PMat
pmspec(struct PMat m, char *x, int ar, int c)
{
	static struct Pat glob = { .np = -1 };    /* A glob (match everything) pattern. */
	const size_t sppat=sizeof (struct Pat *);
	int i, j, k;
	struct PMat n;

	assert(c>=0 && c<m.c && m.r>0);
	for (n.r=i=0; i<m.r; i++) {
		if (m.m[i][c]->np<0 || m.m[i][c]->c==x)
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
		if (m.m[k][c]->np<0 || m.m[k][c]->c==x)
			n.rs[i++]=m.rs[k];
	if (n.c==0) {
		n.m=0;
		return n;
	}
	n.m=dkalloc(n.r*sizeof *n.m);

	for (k=i=0; i<n.r; k++) {
		struct Pat **ps=m.m[k];
		if (ps[c]->np<0) {
			n.m[i]=dkalloc(n.c*sppat);
			memcpy(n.m[i], ps, c*sppat);
			for (j=0; j<ar; j++)
				n.m[i][c+j]=&glob;
			memcpy(n.m[i]+c+ar, ps+c+1, (n.c-c-ar)*sppat);
			i++;
		} else if (ps[c]->c==x) {
			n.m[i]=dkalloc(n.c*sppat);
			memcpy(n.m[i], ps, c*sppat);
			memcpy(n.m[i]+c, ps[c]->ps, ar*sppat);
			memcpy(n.m[i]+c+ar, ps+c+1, (n.c-c-ar)*sppat);
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
	const size_t sppat=sizeof (struct Pat *);
	int i, k;
	struct PMat n;

	assert(c<m.c);
	for (n.r=0, i=0; i<m.r; i++)
		if (m.m[i][c]->np<0)
			n.r++;
	n.c=m.c-1;
	if (n.r==0) {
		n.rs=0;
		n.m=0;
		return n;
	}
	n.rs=dkalloc(n.r*sizeof *n.rs);
	for (k=i=0; i<n.r; k++)
		if (m.m[k][c]->np<0)
			n.rs[i++]=m.rs[k];
	if (n.c==0) {
		n.m=0;
		return n;
	}
	n.m=dkalloc(n.r*sizeof *n.m);

	for (k=i=0; i<n.r; k++)
		if (m.m[k][c]->np<0) {
			n.m[i]=dkalloc(n.c*sppat);
			memcpy(n.m[i], m.m[k], c*sppat);
			memcpy(n.m[i]+c, m.m[k]+c+1, (n.c-c)*sppat);
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
static void gpterm(struct Pat *);
static void gpcode(struct Pat *);

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
 * rule r.
 */
static void
glocals(struct Rule *r)
{
	int v;

	if (r->elen==0)
		return;
	emit("local ");
	emit("%s", gname(C, r->vpa[0].x));
	for (v=1; v<r->elen; v++)
		emit(", %s", gname(C, r->vpa[v].x));
	emit(" = ");
	gpath(r->vpa[0].p);
	for (v=1; v<r->elen; v++) {
		emit(", ");
		gpath(r->vpa[v].p);
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
		const int arity=crs->s[0].l->nd+crs->s[0].l->np;
		emit("return { ck = ccon, ccon = \"%s\", args = { ", crs->x);
		for (sep=0, i=1; i<=arity; i++, sep=1)
			emit(sep ? ", y%d" : "y%d", i);
		emit(" } }");
		return;
	}
	for (c=0; c<pm.c; c++)
		if (pm.m[0][c]->np>=0)
			break;
	if (c==pm.c) {
		assert(pm.rs[0]<crs->i);
		glocals(&crs->s[pm.rs[0]]);
		emit("return ");
		gcode(crs->s[pm.rs[0]].r);
		return;
	}

	p=pm.m[0][c];
	gcond(p->loc, p->c);
	m=pmspec(pm, p->c, p->np, c);
	grules(m);

	for (i=1; i<pm.r; i++) {
		p=pm.m[i][c];
		if (p->np<0)
			continue;
		emit("\nelse");
		gcond(p->loc, p->c);
		m=pmspec(pm, p->c, p->np, c);
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
	emit(iskind(t)?"chkkind(":"chktype(");
	gterm(t);
	emit(")\n%s = { ck = ccon, ccon = \"%s\", args = { } }\n", gname(C, x), x);
	emit("%s = { tk = tbox, tbox = { ", gname(T, x));
	gcode(t);
	emit(", %s } }\n", gname(C, x));
	emit("chkend(\"%s\")\n", x);
}

/* genrules - Generate code to type check a rule set.
 */
void
genrules(struct RSet *rs)
{
	int i, ar;
	struct PMat pm;

	assert(rs->i>0);
	ar=rs->s[0].l->nd+rs->s[0].l->np;
	crs=rs;

	if (ar==0) {
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
		gpterm(rs->s[i].l);
		emit(")\nchk(");
		gterm(rs->s[i].r);
		emit(", ty)\nend\nchkend(\"rule %d\")\n", i+1);
	}
	emit("chkend(\"rules of %s\")\nend\ncheck_rules()\n", rs->x);
	emit("--[[ Compiling rules of %s. ]]\n", rs->x);
	emit("%s = { ck = clam, arity = %d, args = { }, clam =\n", gname(C, rs->x), ar);
	emit("function (y1");
	for (i=2; i<=ar; i++)
		emit(", y%d", i);
	emit(")\n");
	pm=pmnew(rs);
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
#define MAXID 2*IDLEN+1

/* internal gname - Generate the variable name to be emitted to
 * access a variable. This name can be of two kinds, either a
 * 'code' name or a 'term' name.
 * Warning, this returns a static buffer.
 * Warning, this must be called _only_ when x is an atom,
 * calling it with a static string will cause undefined
 * behaviour.
 */
static inline char *
gname(enum NameKind nt, char *x)
{
	static char s[MAXID], *p, *q;

	q=x+aqual(x);
	for (p=s; x<q; p++, x++)
		*p=*x;
	for (; *x; x++, p++) {
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
		     gname(C, t->upi.x ? t->upi.x : adummy));
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
		gterm(t->upi.ty);
		emit(", ");
		gcode(t->upi.ty);
		x = t->upi.x ? t->upi.x : adummy;
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

/* internal gpcode - Emit the expression representing the dynamic
 * translation of a pattern in the lambda-Pi calculus.
 */
static void
gpcode(struct Pat *p)
{
	int i;

	if (p->np<0) {
		emit("%s", gname(C, p->c));
		return;
	}
	for (i=0; i<p->nd+p->np; i++)
		emit("ap(");
	emit("%s", gname(C, p->c));
	for (i=0; i<p->nd; i++) {
		emit(", ");
		gcode(p->ds[i]);
		emit(")");
	}
	for (i=0; i<p->np; i++) {
		emit(", ");
		gpcode(p->ps[i]);
		emit(")");
	}
}

/* internal gpterm - Emit the expression representing the static
 * translation of a pattern in the lambda-Pi calculus.
 */
static void
gpterm(struct Pat *p)
{
	int i;

	if (p->np<0) {
		emit("%s", gname(T, p->c));
		return;
	}
	for (i=0; i<p->nd+p->np; i++)
		emit("{ tk = tapp, tapp = { ");
	emit("%s", gname(T, p->c));
	for (i=0; i<p->nd; i++) {
		emit(", ");
		gterm(p->ds[i]);
		emit(", ");
		gcode(p->ds[i]);
		emit(" } }");
	}
	for (i=0; i<p->np; i++) {
		emit(", ");
		gpterm(p->ps[i]);
		emit(", ");
		gpcode(p->ps[i]);
		emit(" } }");
	}
}
