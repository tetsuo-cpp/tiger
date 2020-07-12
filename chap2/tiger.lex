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
int commentNesting = 0;

%}

%x comment
%x string
digit [0-9]
id [a-zA-Z][a-zA-Z0-9_]*
space [ \t\r]+

%%
{space}	 {adjust(); continue;}
\n	 {adjust(); EM_newline(); continue;}

 /* Symbols. */
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

 /* Keywords. */
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

 /* Number literals. */
{digit}+ {
    adjust();
    yylval.ival = atoi(yytext);
    return INT;
}

 /* Identifiers. */
{id} {
    adjust();
    yylval.sval = String(yytext);
    return ID;
}

 /* Beginning of a comment. */
"/*" {
    adjust();
    BEGIN comment;
    ++commentNesting;
}
 /* End of a comment outside of a comment. */
"*/" {
    adjust();
    EM_error(EM_tokPos, "close comment without a corresponding open");
}

 /* Beginning of a string literal. */
"\"" {adjust(); BEGIN string;}

. {adjust(); EM_error(EM_tokPos,"illegal token");}

 /* Comment rules. */
<comment>{
    /* Nested comment. */
    "/*" {
        adjust();
        ++commentNesting;
    }
    "*/" {
        adjust();
        --commentNesting;
        if (commentNesting == 0)
            BEGIN INITIAL;
    }
    <<EOF>> {
        adjust();
        EM_error(EM_tokPos, "encountered eof within a comment");
        yyterminate();
    }
    . {adjust(); continue;}
}

 /* String literal rules. */
<string>{
    "\\n" {adjust(); stringLiteralPushChar('\n');}
    "\\t" {adjust(); stringLiteralPushChar('\t');}
    "\\\"" {adjust(); stringLiteralPushChar('\"');}
    "\\\\" {adjust(); stringLiteralPushChar('\\');}
    "\\"{digit}{digit}{digit} {adjust(); stringLiteralPushChar(atoi(&yytext[1]));}
    "\\"{digit}+ {
        adjust();
        EM_error(EM_tokPos, "illegal ascii code");
    }
    "\"" {
        adjust();
        stringLiteralTerminate();
        yylval.sval = String(stringLiteral);
        BEGIN INITIAL;
        return STRING;
    }
    /* Control characters. */
    "\^"[@-_] {
        adjust();
        stringLiteralPushChar('@' - yytext[1]);
    }
    /* The DEL control character is a special case as it's in a different range from the rest. */
    "\^?" {
        adjust();
        stringLiteralPushChar(127);
    }
    /* We're allowed to put whitespace between backslashes to allow multiline string literals. */
    "\\"[ \t\n\r]+"\\" {
        adjust();
        for (int i  = 0; yytext[i] != 0; ++i) {
            if (yytext[i] == '\n')
                EM_newline();
        }
    }
    "\\" {adjust(); EM_error(EM_tokPos,"illegal escape sequence");}
    <<EOF>> {
        adjust();
        EM_error(EM_tokPos, "encountered eof within a string literal");
        yyterminate();
    }
    . {adjust(); stringLiteralPushChar(yytext[0]);}
}
