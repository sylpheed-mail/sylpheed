/* The following defines shamelessly stolen from GDB sources... */

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth quote_fmtmaxdepth
#define	yyparse	quote_fmtparse
#define	yylex	quote_fmtlex
#define	yyerror	quote_fmterror
#define	yylval	quote_fmtlval
#define	yychar	quote_fmtchar
#define	yydebug	quote_fmtdebug
#define	yypact	quote_fmtpact	
#define	yyr1	quote_fmtr1			
#define	yyr2	quote_fmtr2			
#define	yydef	quote_fmtdef		
#define	yychk	quote_fmtchk		
#define	yypgo	quote_fmtpgo		
#define	yyact	quote_fmtact		
#define	yyexca	quote_fmtexca
#define yyerrflag quote_fmterrflag
#define yynerrs	quote_fmtnerrs
#define	yyps	quote_fmtps
#define	yypv	quote_fmtpv
#define	yys	quote_fmts
#define	yy_yys	quote_fmtyys
#define	yystate	quote_fmtstate
#define	yytmp	quote_fmttmp
#define	yyv	quote_fmtv
#define	yy_yyv	quote_fmtyyv
#define	yyval	quote_fmtval
#define	yylloc	quote_fmtlloc
#define yyreds	quote_fmtreds		/* With YYDEBUG defined */
#define yytoks	quote_fmttoks		/* With YYDEBUG defined */
#define yylhs	quote_fmtyylhs
#define yylen	quote_fmtyylen
#define yydefred quote_fmtyydefred
#define yydgoto	quote_fmtyydgoto
#define yysindex quote_fmtyysindex
#define yyrindex quote_fmtyyrindex
#define yygindex quote_fmtyygindex
#define yytable	 quote_fmtyytable
#define yycheck	 quote_fmtyycheck
