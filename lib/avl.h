/* Balanced tree functions.
 *
 * The implementation was taken from:
 *   http://h.lmbd.fr/cbits/avl.c
 */

struct Tree;
struct Tree *avlnew(int (*)(void *, void *), void (*)(void *));
void avlfree(struct Tree *);
void *avlget(void *, struct Tree *);
void avlins(void *, struct Tree *);
