#include <stdlib.h>
/* Dk types and functions. */

/* IDLEN - The maximum size of a fully qualified
 * identifier. This includes the terminating nul.
 */
#define IDLEN 128

/* struct Term - Terms of the lambda-Pi calculus are
 * classically represented as a tag and an union. 
 */
struct Term {
	enum { App, Lam, Pi, Var, Type } typ;
	union {
		struct {
			struct Term *t1;
			struct Term *t2;
		} app;
		struct {
			char *x;
			struct Term *t;
		} lam;
		struct {
			char *x;
			struct Term *ty;
			struct Term *t;
		} pi;
		char *var;
	} u;
};

#define uapp u.app
#define ulam u.lam
#define upi  u.pi
#define uvar u.var

/* struct Pat - Patterns are construcors applied first
 * to a list of nd dot patterns stored in the array ds,
 * then to a list of np patterns stored in the array
 * ps.
 * The loc field is a 0 terminated array of integers
 * representing the location of the current constructor
 * in a bigger pattern.
 * As a convention, if np is negative, the pattern is a
 * variable (but this cannot be detected during the
 * parsing). The contents of the other fields is then
 * undefined.
 */
struct Pat {
	char *c;
	int nd, np, *loc;
	struct Term **ds;
	struct Pat **ps;
};

/* struct VPth - This structure binds a pattern variable
 * to its path in the global rule pattern. A path is a 0
 * terminated array of integers.
 * For instance, the path of x in:
 *
 *     plus y (S x)
 *
 * is { 2, 1, 0 }.
 */
struct VPth {
	char *x;
	int *p;
};

struct Rule {
	struct VPth *vpa;
	int elen;
	struct Env *e;
	struct Pat *l;
	struct Term *r;
};

#define MAXRULES 32
struct RSet {
	struct Rule s[MAXRULES];
	int i;
	char *x;
};

/* Module alloc.c */
void *xalloc(size_t);
void *xrealloc(void *, size_t);
void initalloc(void);
void deinitalloc(void);
extern char *adummy;
char *astrdup(const char *, int);
int aqual(const char *);
void *dkalloc(size_t);
void dkfree(void);

/* Module term.c */
extern struct Term *ttype;
struct Term *mkapp(struct Term *, struct Term *);
struct Term *mkvar(char *);
struct Term *mklam(char *, struct Term *);
struct Term *mkpi(char *, struct Term *, struct Term *);
int napps(struct Term *, struct Term **);
int iskind(struct Term *);

/* Module pat.c */
struct PArr;
struct PArr *pains(struct PArr *, void *);
struct Pat *mkpat(char *, struct PArr *, struct PArr *);
int pchk(struct Rule *);
void pdone(void);

/* Module scope.c */
struct Env;
enum IdStatus { DECL, DEF };
void initscope(void);
void deinitscope(void);
void pushscope(char *);
int tscope(struct Term *, struct Env *);
int pscope(struct Pat *, struct Env *);
enum IdStatus chscope(char *, enum IdStatus);
struct Env *eins(struct Env *, char *, struct Term *);
struct Term *eget(struct Env *, char *);
void eiter(struct Env *, void (*)(char *, struct Term *, void *), void *);
size_t elen(struct Env *);
int escope(struct Env *);

/* Module rule.c */
int pushrule(struct Env *, struct Pat *, struct Term *);
void dorules(void);

/* Module gen.c */
void genmod(void);
void genrules(struct RSet *);
void gendecl(char *, struct Term *);

/* Module module.c */
int mset(char *);
const char *mget(void);
char *mqual(char *);
