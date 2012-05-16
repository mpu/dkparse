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
	struct Term *term;
	struct Env *env;
}

%token ARROW
%token FATARROW
%token LONGARROW
%token TYPE
%token <id> ID

%type <term> simpl
%type <term> app
%type <term> term
%type <env> bdgs

%start top

%right ARROW FATARROW

%%
top: /* empty */ { YYABORT; }
   | decl '.'    { dkfree(); YYACCEPT; }
   | rules '.'   { dorules(); YYACCEPT; }
;

rules: rule
     | rules rule
;

decl: ID ':' term {
	if (scope($3, 0)) {
		fprintf(stderr, "%s: Scope error in type of %s.\n", __func__, $1);
		exit(1);
	}
	pushscope($1);
};

rule: '[' bdgs ']' term LONGARROW term { pushrule($2, $4, $6); }
;

bdgs: /* empty */          { $$ = 0; }
    | bdgs ',' ID ':' term { $$ = eins($1, $3, $5); }
    | ID ':' term          { $$ = eins(0, $1, $3); }
;

simpl: ID           { $$ = mkvar($1); }
     | '(' term ')' { $$ = $2; }
     | TYPE         { $$ = ttype; }
;

app: simpl     { $$ = $1; }
   | app simpl { $$ = mkapp($1, $2); }
;

term: app                      { $$ = $1; }
    | ID ':' app ARROW term    { $$ = mkpi($1, $3, $5); }
    | term ARROW term          { $$ = mkpi(0,  $1, $3); }
    | ID ':' app FATARROW term { $$ = mklam($1, $5); }
    | ID FATARROW term         { $$ = mklam($1, $3); }
;
%%

static int
peek(FILE *f)
{
	int c = fgetc(f);
	if (c!=EOF) ungetc(c, f);
	return c;
}

static int
skipspaces(FILE *f)
{
	int c;
	while ((c=fgetc(f))!=EOF && isspace(c));
	return c;
}

#define isspecial(c) (c=='[' || c==']' || c==',' || c=='.' || c==':' || c=='(' || c==')')
static int
lex(void)
{
	static char tok[TOKLEN];
	int l, c;

	c=skipspaces(f);
	if (c=='(' && peek(f)==';') {
		while ((fgetc(f)!=';' || peek(f)!=')') && !feof(f));
		fgetc(f); /* Drop trailing ')'. */
		c=skipspaces(f);
	}
	if (c==EOF)
		return 0;
	if (isspecial(c))
		return c;
	l=0;
	do {
		if (l>=TOKLEN-1) {
			fputs("Maximum token length exceeded.\n", stderr);
			exit(1);
		}
		tok[l++]=c;
		c=fgetc(f);
	} while ((!isspace(c) && !isspecial(c)) || (c=='.' && !isspace(peek(f))));
	ungetc(c, f);
	tok[l]=0;
	     if (!strcmp(tok, "->"))
		return ARROW;
	else if (!strcmp(tok, "=>"))
		return FATARROW;
	else if (!strcmp(tok, "-->"))
		return LONGARROW;
	else if (!strcmp(tok, "Type"))
		return TYPE;
	yylval.id=astrdup(tok);
	return ID;
}

static int
yylex(void)
{
	int i = lex();
#if 0
	char b[2] = { 0, 0 };
	char *ty;
	switch (i) {
	case ARROW: ty = "ARROW"; break;
	case FATARROW: ty = "FATARROW"; break;
	case LONGARROW: ty = "LONGARROW"; break;
	case TYPE: ty = "TYPE"; break;
	case ID: ty = "ID"; break;
	default: ty = b; b[0]=i; break;
	}
	fprintf(stderr, "lex: '%s'\n", ty);
#endif
	return i;
}

static void
yyerror(const char *m)
{
	printf("Yacc error, %s.\n", m);
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
		printf("Parsing %s.\n", *argv);
		while (yyparse()==0);
		fclose(f);
	}
	deinitscope();
	deinitalloc();
	exit(0);
}
