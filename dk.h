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
