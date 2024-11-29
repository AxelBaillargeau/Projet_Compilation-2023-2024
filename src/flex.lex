%{
/* instructions tpc.lex */
#include "tpc-bison.h"
int lineno=1;
int charno=1;
char lastWord[256];
%}

%option noinput nounput noyywrap warn
%x startcomment characterRead

digit [0-9]
letter [a-zA-Z]
IdChar {letter}|_
nonId (?:[^a-zA-Z_]|[^0-9])

%%
<*>\n {++lineno;charno=1;};

<*>[[:blank:]]+ {charno+=strlen(yytext);};

<*>[/]{2}.* {charno+=strlen(yytext);};
<*>\/\* {BEGIN(startcomment);charno+=2;};
<startcomment>\*\/ {BEGIN(0);charno+=2;};
<startcomment>. {++charno;};

if/{nonId} {charno+=2;strcpy(lastWord,"if");return IF;};
else/{nonId} {charno+=4;strcpy(lastWord,"else");return ELSE;};
while/{nonId} {charno+=5;strcpy(lastWord,"while");return WHILE;};
return/{nonId} {charno+=6;strcpy(lastWord,"return");return RETURN;};
void {charno+=4;strcpy(lastWord,"void");return VOID;};

("int"|"char") {
    if (yytext[0] == 'i') {strcpy(yylval.type,"int");charno+=3;}
    else {strcpy(yylval.type,"char");charno+=4;}
    strcpy(lastWord,yytext);
    return TYPE;
}

\' {charno+=1;BEGIN(characterRead);strcpy(lastWord,"\'");}

<characterRead>(.|\\.)/\' { // on lit un caractère dans une chaîne de caractères
    if (strlen(yytext) >= 2) {
        switch (yytext[1]) {
            case 'a': yylval.byte = '\a'; break;
            case 'b': yylval.byte = '\b'; break;
            case 'f': yylval.byte = '\f'; break;
            case 'n': yylval.byte = '\n'; break;
            case 'r': yylval.byte = '\r'; break;
            case 't': yylval.byte = '\t'; break;
            case 'v': yylval.byte = '\v'; break;
            case '0': yylval.byte = '\0'; break;
            default: yylval.byte = yytext[1]; break;
        }
    }
    else 
        yylval.byte = yytext[0];
    
    charno+=strlen(yytext);
    strcpy(lastWord, yytext);
    return CHARACTER;
};

<characterRead>\' {charno++;BEGIN(0);strcpy(lastWord,"\'");};


({IdChar}({IdChar}|{digit})*) {
    strcpy(yylval.ident, yytext);
    charno+=strlen(yytext);
    strcpy(lastWord,yytext);
    return IDENT;
};
{digit}* {
    yylval.num = atoi(yytext);
    strcpy(lastWord,yytext);
    charno+=strlen(yytext);
    return NUM;
    };
[+-] {
    yylval.byte = yytext[0];
    strcpy(lastWord,yytext);
    charno++;
    return ADDSUB;
}
[/*%] {
    yylval.byte = yytext[0];
    strcpy(lastWord,yytext);
    charno++;
    return DIVSTAR;
};
([><]=)|[><] {
    strcpy(yylval.comp, yytext);
    strcpy(lastWord,yytext);
    charno+=strlen(yytext);
    return ORDER;
};
[=!]= {
    strcpy(yylval.comp, yytext);
    strcpy(lastWord,yytext);
    charno++;
    return EQ;
};
[&]{2} {charno+=2;strcpy(lastWord,yytext);return AND;}
[|]{2} {charno+=2;strcpy(lastWord,yytext);return OR;}

<*>. {
    strcpy(lastWord,yytext);
    ++charno;
    return yytext[0];
};

<startcomment><<EOF>> {
    strcpy(lastWord,yytext);
    return 1;
    };

<<EOF>> {
    return 0;
    };

%%
int readFile(char *filename) {
    FILE *f = fopen(filename,"r");
    if (f == NULL) {
        fprintf(stderr,"Error : file not found\n");
        return 2;
    }
    yyin = f;
    /* J'ai pas compris comment marchait yyrestart() */
    return 0;
}