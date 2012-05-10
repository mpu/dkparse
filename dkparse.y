%{
  #include <ctype.h>
  #include <stdlib.h>
  #include <stdio.h>
  #include <string.h>
  #define YYSTYPE char *
  #define TOKLEN 128

  static FILE *f;
  static int yylex(void);
  static void yyerror(const char *);
%}

%token ARROW
%token FATARROW
%token LONGARROW
%token TYPE
%token ID

%start top

%right ARROW FATARROW

%%
top: /* empty */ { YYABORT; }
   | decl
   | rule
;

decl: ID ':' term '.' {
	printf("Read one declaration.\n");
	YYACCEPT;
};

rule: ctx term LONGARROW term '.' {
	printf("Read one rewrite rule.\n");
	YYACCEPT;
};

ctx: '[' bdgs ']'
;

bdgs: /* empty */
    | bdgs ',' ID ':' term
    | ID ':' term
;

simpl: ID
     | '(' term ')'
     | TYPE
;

app: simpl
   | app simpl
;

term: app
    | ID ':' app ARROW term
    | term ARROW term
    | ID ':' app FATARROW term
    | ID FATARROW term
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
	yylval=tok;
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
	while (argv++, --argc) {
		if (!(f=fopen(*argv, "r"))) {
			fprintf(stderr, "Cannot open %s.\n", *argv);
			continue;
		}
		printf("Parsing %s.\n", *argv);
		while (yyparse()==0)
			;
		fclose(f);
	}
	exit(0);
}
