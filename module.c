#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "dk.h"
#define IDLEN 256

/* DIRSEP - Directory separator.
 */
#define DIRSEP '/'

/* internal qid id - The static array qid is used to store
 * names qualified by this module, its first part is current
 * the module name, then identifiers can be concatenated to
 * get their qualified version. The pointer id points at the
 * end of the module name stored in the beginning of qid.
 */
static char qid[IDLEN];
static char *id;

/* internal copyto - Copy chars between two string pointers
 * after the s pointer in the qid array. The s pointer must
 * point inside the qid array. If a non null pointer is
 * returned, it will point after the string copied, at least
 * 1 before the last qid char.
 */
static inline char *
copyto(char *s, char *f, char *t)
{
	if (!isalpha(*f))
		goto err;
	if (t-f>IDLEN-2-(s-qid)) {
		fprintf(stderr, "%s: Maximum qualified id length exceeded.\n"
		                "\tThe maximum is %d.\n", __func__, IDLEN);
		return 0;
	}
	while (f<t) {
		if (!isalnum(*f) && *f!='_' && *f!=0)
			goto err;
		*s++=*f++;
	}
	return s;
err:
	fprintf(stderr, "%s: Invalid char %c in module name.\n", __func__, *f);
	return 0;
}

/* mset - Set the current module by translating the file path
 * into a module hierarchy. If the call succeeds, 0 is returned,
 * in any other case, 1 is returned.
 * After calling this function (if 0 is returned) all calls to
 * mqual will return names qualified by the new current module.
 */
int
mset(char *path)
{
	char *p;

	id=qid;
	while ((p=strchr(path, DIRSEP))) {
		id=copyto(id, path, p);
		if (!id)
			return 1;
		*id++='.';
		path=p+1;
	}
	p=strchr(path, '.');
	if (!p || strcmp(p, ".dk"))
		fprintf(stderr, "%s: Warning, path %s should end in '.dk'.\n"
		              , __func__, path);
	if (!p)
		p=path+strlen(path);
	id=copyto(id, path, p);
	return !id;
}

/* mget - Get the current module name. It returns a pointer on
 * a static null terminated string.
 */
const char *
mget(void)
{
	*id=0;
	return qid;
}

/* mqual - Qualify a variable name. It returns a pointer on
 * an atom containing the qualified name.
 */
char *
mqual(char *x)
{
	*id='.';
	if (!copyto(id+1, x, x+strlen(x)+1))
		exit(1); // FIXME
	return astrdup(qid);
}
