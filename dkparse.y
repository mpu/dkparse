%{
  #include <ctype.h>
  #include <stdlib.h>
  #include <stdio.h>
  #include <string.h>
  #include "dk.h"
  #define TOKLEN 128

  static FILE *f;
  static int yylex(void);
  static void yyerror(const char *);
%}

%union {
	char *id;
	struct Pat *pat;
	struct Term *term;
	struct Env *env;
	struct PArr *parr;
}

%token ARROW
%token FATARROW
%token LONGARROW
%token TYPE
%token <id> ID

%type <parr> dotps
%type <parr> spats
%type <pat> spat
%type <pat> pat
%type <term> sterm
%type <term> app
%type <term> term
%type <env> bdgs

%start top

%right ARROW FATARROW

%%
top: /* empty */   { }
   | top decl '.'  { dkfree(); }
   | top rules '.' { dorules(); }
;

rules: rule
     | rules rule
;

decl: ID ':' term {
	char *id;

	id=mqual($1);
	if (tscope($3, 0)) {
		fprintf(stderr, "%s: Scope error in type of %s.\n", __func__, $1);
		exit(1); // FIXME
	}
	gendecl(id, $3);
	pushscope(id);
};

rule: '[' bdgs ']' pat LONGARROW term { pushrule($2, $4, $6); }
;

bdgs: /* empty */          { $$ = 0; }
    | bdgs ',' ID ':' term { $$ = eins($1, $3, $5); }
    | ID ':' term          { $$ = eins(0, $1, $3); }
;

pat: ID dotps spats { $$ = mkpat($1, $2, $3); }
;

dotps: /* empty */        { $$ = 0; }
     | dotps '{' term '}' { $$ = pains($1, $3); }
;

spats: /* empty */ { $$ = 0; }
     | spats spat  { $$ = pains($1, $2); }
;

spat: ID          { $$ = mkpat($1, 0, 0); }
    | '(' pat ')' { $$ = $2; }
;

sterm: ID           { $$ = mkvar($1); }
     | '(' term ')' { $$ = $2; }
     | TYPE         { $$ = ttype; }
;

app: sterm     { $$ = $1; }
   | app sterm { $$ = mkapp($1, $2); }
;

term: app                      { $$ = $1; }
    | ID ':' app ARROW term    { $$ = mkpi($1, $3, $5); }
    | term ARROW term          { $$ = mkpi(0,  $1, $3); }
    | ID ':' app FATARROW term { $$ = mklam($1, $5); }
    | ID FATARROW term         { $$ = mklam($1, $3); }
;
%%

static int
peek(void)
{
	int c = fgetc(f);
	if (c!=EOF) ungetc(c, f);
	return c;
}

static int
skipspaces(void)
{
	int c;
	while (1) {
		while ((c=fgetc(f))!=EOF && isspace(c));
		if (c!='(' || peek()!=';')
			return c;
		while ((fgetc(f)!=';' || peek()!=')') && !feof(f));
		fgetc(f); /* Drop trailing ')'. */
	}
}

#define istoken(c) (isalnum(c) || c=='_' || c=='\'')
static int
yylex(void)
{
	static char tok[TOKLEN];
	int l, c;

	c=skipspaces();
	if (c==EOF)
		return 0;
	if (strchr("[]{}(),.:", c))
		return c;
	if (c=='-' || c=='=') {
		switch (fgetc(f)) {
		case '>':
			return c=='-' ? ARROW : FATARROW;
		case '-':
			if (c=='-' && fgetc(f)=='>')
				return LONGARROW;
			break;
		}
		return c; /* This is an error. */
	}
	for (l=0; c!=EOF && (istoken(c) || (c=='.' && istoken(peek()))); l++) {
		if (l>=TOKLEN-1) {
			fputs("Maximum token length exceeded.\n", stderr);
			exit(1);
		}
		tok[l]=c;
		c=fgetc(f);
	}
	tok[l]=0;
	if (!l)
		return c; /* This is an error. */
	if (c!=EOF)
		ungetc(c, f); /* Push back last char. */
	if (strcmp(tok, "Type")==0)
		return TYPE;
	yylval.id=astrdup(tok);
	return ID;
}

static void
yyerror(const char *m)
{
	fprintf(stderr, "Yacc error, %s.\n", m);
	exit(1);
}

int
main(int argc, char **argv)
{
	if (argc<2) {
		fputs("usage: dkparse FILES\n", stderr);
		exit(1);
	}
	initalloc();
	initscope();
	while (argv++, --argc) {
		if (!(f=fopen(*argv, "r"))) {
			fprintf(stderr, "Cannot open %s.\n", *argv);
			continue;
		}
		if (mset(*argv)) {
			fprintf(stderr, "Invalid module name %s.\n", *argv);
			continue;
		}
		fprintf(stderr, "Parsing module %s.\n", mget());
		genmod();
		yyparse();
		fclose(f);
	}
	deinitscope();
	deinitalloc();
	exit(0);
}
