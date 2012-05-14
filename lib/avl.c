/* avl
 * A trivial implementation of
 * AVL trees in C.
 *
 * This code was taken from:
 *   http://h.lmbd.fr/cbits/avl.c
 */
#include <stdlib.h>
#include "avl.h"

/* WARNING:
 * Declaring xalloc here is dangerous, the original
 * declaration is in dk.h, the two must keep in
 * sync.
 */
void *xalloc(size_t);
#define xfree free

struct Tree {
	int (*cmp)(void *, void *);
	void (*free)(void *);
	struct N {
		void *p;
		signed char bal;
		struct N *n[2];
	} *root;
};

/* avlnew - Create an empty AVL tree and return it.
 * A comparison function must be provided, elements of the
 * tree will be compared with this function.
 */
struct Tree *
avlnew(int (*cmp)(void *, void *), void (*free)(void *))
{
	struct Tree *t=xalloc(sizeof *t);
	t->cmp=cmp;
	t->free=free;
	t->root=0;
	return t;
}

/* avlfree - Free the memory allocated by a search
 * tree. Works by successive rotations, as explained
 * in http://www.stanford.edu/~blp/avl/libavl.html.
 */
void
avlfree(struct Tree *t)
{
	struct N *p, *n=t->root;

	for (; n; n=p) {
		for (; (p=n->n[0]); n=p) {
			n->n[0]=p->n[1];
			p->n[1]=n;
		}
		p=n->n[1];
		if (t->free)
			t->free(n->p);
		xfree(n);
	}
	xfree(t);
}

/* avlget - Get an element from the AVL tree.
 */
void *
avlget(void *p, struct Tree *t)
{
	struct N *n=t->root;

	while (n) {
		int c=t->cmp(p, n->p);
		if (c==0)
			return p;
		n=n->n[c>0];
	}
	return 0;
}

/* internal ins - Insert one data into the AVL tree,
 * keeping the tree balanced using the usual rotating
 * algoritm.
 * Returns 1 if the size of the subtree grew,
 * 0 otherwise.
 */
static int
ins(void *p, struct N **pn, int (*cmp)(void *, void *))
{
	int c;
	struct N *n=*pn;

	if (!n) {
		/* We found an appropriate location
		 * to insert the element, in this case
		 * the subtree is no longer free, hence
		 * we return 1 to signal it.
		 */
		struct N *node=xalloc(sizeof *node);
		node->p=p;
		node->n[0]=node->n[1]=0;
		node->bal=0;
		*pn=node;
		return 1;
	}
	c=cmp(p, n->p);
	if (c<0) {
		n->bal-=ins(p, &n->n[0], cmp);
		if (n->bal==-2) {
			/* Here the size of the left subtree
			 * is too big, we correct this by
			 * performing some local rotations.
			 */
			struct N *a=n->n[0];
			if (a->bal==-1) {
				/* In this case we simply need
				 * a right rotation to correct
				 * the balances.
				 *     n        a
				 *    / \      / \
				 *   a   3 -> 1   n
				 *  / \          / \
				 * 1   2        2   3
				 */
				*pn=a;
				n->n[0]=a->n[1];
				a->n[1]=n;
				a->bal=n->bal=0;
			} else {
				/* This case is trickier, we
				 * need two rotations to pull
				 * the longest branch on top.
				 *     n          b
				 *    / \        / \
				 *   a   4      a   \
				 *  / \    ->  / \   n
				 * 1   b      1   2 / \
				 *    / \          3   4
				 *   2   3
				 */
				struct N *b=a->n[1];
				*pn=b;
				a->n[1]=b->n[0];
				n->n[0]=b->n[1];
				b->n[0]=a;
				b->n[1]=n;
				if (b->bal<0)
					a->bal=0, n->bal=1;
				else if (b->bal>0)
					a->bal=-1, n->bal=0;
				else
					a->bal=0, n->bal=0;
				b->bal=0;
			}
			return 0;
		}
		return n->bal<0;
	}
	if (c>0) {
		n->bal+=ins(p, &n->n[1], cmp);
		if (n->bal==2) {
			/* The symetric case.
			 */
			struct N *a=n->n[1];
			if (a->bal==1) {
				*pn=a;
				n->n[1]=a->n[0];
				a->n[0]=n;
				a->bal=n->bal=0;
			} else {
				struct N *b=a->n[0];
				*pn=b;
				a->n[0]=b->n[1];
				n->n[1]=b->n[0];
				b->n[1]=a;
				b->n[0]=n;
				if (b->bal>0)
					a->bal=0, n->bal=-1;
				else if (b->bal<0)
					a->bal=1, n->bal=0;
				else
					a->bal=0, n->bal=0;
				b->bal=0;
			}
			return 0;
		}
		return n->bal>0;
	}
	/* If c is 0 then the tree is left
	 * unchanged and 0 is returned.
	 */
	return 0;
}

/* avlins - Public interfarce for ins.
 */
void
avlins(void *p, struct Tree *t)
{
	ins(p, &t->root, t->cmp);
}
