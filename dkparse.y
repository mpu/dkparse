%{
  #include <ctype.h>
  #include <stdlib.h>
  #include <stdio.h>
  #include <string.h>
  #include "dk.h"

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

static int
istoken(int c)
{
	return isalnum(c) || c=='_' || c=='\'';
}

static int
yylex(void)
{
	static char tok[IDLEN];
	int l, c, qual;

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
	for (qual=l=0; c!=EOF && (istoken(c) || (c=='.' && istoken(peek()))); l++) {
		if (l>=IDLEN-1) {
			fputs("Maximum identifier length exceeded.\n", stderr);
			exit(1);
		}
		if (c=='.')
			qual=l+1;
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
	yylval.id=astrdup(tok, qual);
	return ID;
}

static void
yyerror(const char *m)
{
	fprintf(stderr, "Yacc error, %s.\n", m);
	exit(1);
}

static int
opengfile(char *mod)
{
	char *f, *p;

	f=xalloc(strlen(mod)+5);
	strcpy(f, mod);
	if (!(p=strrchr(f, '.')))
		p=f+strlen(f);
	strcpy(p, ".lua");

	if (!(gfile=fopen(f, "w"))) {
		fprintf(stderr, "Cannot open %s.\n", f);
		free(f);
		return 1;
	}
	free(f);
	return 0;
}

int
main(int argc, char **argv)
{
	gmode=Check;
	if (argc<2) {
		printf("usage: %s [-c] FILES\n", argv[0]?argv[0]:"dkparse");
		exit(1);
	}
	if (strcmp(argv[1], "-c")==0) {
		gmode=Compile;
		argv++, --argc;
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
		if (gmode==Compile) {
			if (opengfile(*argv))
				continue;
		} else
			gfile=stdout;
		fprintf(stderr, "Parsing module %s.\n", mget());
		genmod();
		yyparse();
		fclose(f);
		if (gmode==Compile)
			fclose(gfile);
	}
	deinitscope();
	deinitalloc();
	exit(0);
}
