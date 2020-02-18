%{
#include <string.h>
#include "util.h"
#include "tokens.h"
#include "errormsg.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}


void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

#define MAX_STRING_LITERAL_LEN 1024
char stringLiteral[MAX_STRING_LITERAL_LEN];
size_t stringLiteralLen = 0;
void stringLiteralPushChar(char c) {
  assert(stringLiteralLen < MAX_STRING_LITERAL_LEN);
  stringLiteral[stringLiteralLen++] = c;
}
void stringLiteralTerminate() {
  stringLiteral[stringLiteralLen] = 0;
  stringLiteralLen = 0;
}

%}

%x comment
%x string
digits [0-9]+
id [a-zA-Z][a-zA-Z0-9_]*
space [ \t\r]+

%%
{space}	 {adjust(); continue;}
\n	 {adjust(); EM_newline(); continue;}
","	 {adjust(); return COMMA;}
":"	 {adjust(); return COLON;}
";"	 {adjust(); return SEMICOLON;}
"("	 {adjust(); return LPAREN;}
")"	 {adjust(); return RPAREN;}
"["	 {adjust(); return LBRACK;}
"]"	 {adjust(); return RBRACK;}
"{"	 {adjust(); return LBRACE;}
"}"	 {adjust(); return RBRACE;}
"."	 {adjust(); return DOT;}
"+"	 {adjust(); return PLUS;}
"-"	 {adjust(); return MINUS;}
"*"	 {adjust(); return TIMES;}
"/"	 {adjust(); return DIVIDE;}
"="	 {adjust(); return EQ;}
"<>"	 {adjust(); return NEQ;}
"<"	 {adjust(); return LT;}
"<="	 {adjust(); return LE;}
">"	 {adjust(); return GT;}
">="	 {adjust(); return GE;}
"&"	 {adjust(); return AND;}
"|"	 {adjust(); return OR;}
":="	 {adjust(); return ASSIGN;}
while	 {adjust(); return WHILE;}
for  	 {adjust(); return FOR;}
to  	 {adjust(); return TO;}
break	 {adjust(); return BREAK;}
let  	 {adjust(); return LET;}
in  	 {adjust(); return IN;}
end  	 {adjust(); return END;}
function {adjust(); return FUNCTION;}
var	 {adjust(); return VAR;}
type	 {adjust(); return TYPE;}
array	 {adjust(); return ARRAY;}
if  	 {adjust(); return IF;}
then	 {adjust(); return THEN;}
else	 {adjust(); return ELSE;}
do  	 {adjust(); return DO;}
of  	 {adjust(); return OF;}
nil  	 {adjust(); return NIL;}
{digits} {adjust(); yylval.ival=atoi(yytext); return INT;}
{id}	 {adjust(); yylval.sval=String(yytext); return ID;}
"/*"	 {adjust(); BEGIN comment;}
<comment>"*/" {adjust(); BEGIN INITIAL;}
<comment>. {adjust(); continue;}
"\""	 {adjust(); BEGIN string;}
<string>"\\n" {adjust(); stringLiteralPushChar('\n');}
<string>"\\t" {adjust(); stringLiteralPushChar('\t');}
<string>"\\\"" {adjust(); stringLiteralPushChar('\"');}
<string>"\\\\" {adjust(); stringLiteralPushChar('\\');}
<string>"\\" {adjust(); EM_error(EM_tokPos,"illegal escape sequence");}
<string>"\"" {adjust(); stringLiteralTerminate(); yylval.sval=String(stringLiteral); BEGIN INITIAL; return STRING;}
<string>. {adjust(); stringLiteralPushChar(yytext[0]);}
.	 {adjust(); EM_error(EM_tokPos,"illegal token");}
