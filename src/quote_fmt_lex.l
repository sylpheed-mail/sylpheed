%{
#include "quote_fmt_lex.h"
#include "quote_fmt_parse.h"
%}

%option prefix="quote_fmt"
%option outfile="lex.yy.c"

%%

"%d" /* date */ return SHOW_DATE;
"%f" /* from */ return SHOW_FROM;
"%N" /* full name */ return SHOW_FULLNAME;
"%F" /* first name */ return SHOW_FIRST_NAME;
"%I" /* initial of sender */ return SHOW_SENDER_INITIAL;
"%s" /* subject */ return SHOW_SUBJECT;
"%t" /* to */ return SHOW_TO;
"%c" /* cc */ return SHOW_CC;
"%n" /* newsgroups */ return SHOW_NEWSGROUPS;
"%i" /* message-id */ return SHOW_MESSAGEID;
"%r" /* references */ return SHOW_REFERENCES;
"%M" /* message */ return SHOW_MESSAGE;
"%Q" /* quoted message */ return SHOW_QUOTED_MESSAGE;
"%m" /* message with no signature */ return SHOW_MESSAGE_NO_SIGNATURE;
"%q" /* quoted message with no signature */ return SHOW_QUOTED_MESSAGE_NO_SIGNATURE;
"%%" /* % */ return SHOW_PERCENT;
"\\\\" /* \ */ return SHOW_BACKSLASH;
"\\t"|"\t" /* tab */ return SHOW_TAB;
"\\n"|"\n" /* return */ return SHOW_EOL;
"\\?" /* ? */ return SHOW_QUESTION_MARK;
"\\{" return SHOW_OPARENT;
"\\}" return SHOW_CPARENT;
"?d" /* query date */ return QUERY_DATE;
"?f" /* query from */ return QUERY_FROM;
"?N"|"?F"|"?I" /* query from name */ return QUERY_FULLNAME;
"?s" /* query subject */ return QUERY_SUBJECT;
"?t" /* query to */ return QUERY_TO;
"?c" /* query cc */ return QUERY_CC;
"?n" /* query newsgroups */ return QUERY_NEWSGROUPS;
"?i" /* query message-id */ return QUERY_MESSAGEID;
"?r" /* query references */ return QUERY_REFERENCES;
"{" return OPARENT;
"}" return CPARENT;
. { yylval.chr = yytext[0]; return CHARACTER; }

%%
