#include <stdlib.h>
/* Dk types and functions. */

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

struct Rule {
	struct Env *e;
	char *x;
	struct Term *l, *r;
};

#define uapp u.app
#define ulam u.lam
#define upi  u.pi
#define uvar u.var

/* Module alloc.c */
void *xalloc(size_t);
void *xrealloc(void *, size_t);
void initalloc(void);
void deinitalloc(void);
char *astrdup(const char *);
void *dkalloc(size_t);
void dkfree(void);

/* Module term.c */
extern struct Term *ttype;
struct Term *mkapp(struct Term *, struct Term *);
struct Term *mkvar(char *);
struct Term *mklam(char *, struct Term *);
struct Term *mkpi(char *, struct Term *, struct Term *);
char *headv(struct Term *);

/* Module scope.c */
struct Env;
int scope(struct Term *);
struct Env *enew(void);
void eins(struct Env *, char *, struct Term *);
void eiter(struct Env *, void (*)(char *, struct Term *));
