%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include "util.h"
#include "tokens.h"
#include "errormsg.h"

int charPos = 1;

int yywrap(void) {
    charPos = 1;
    return 1;
}

void adjust(void) {
    EM_tokPos = charPos;
    charPos += yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

int commentDepth = 0;

/* @function: getstr
 * @input: a string literal
 * @output: the string value for the input which has all the escape sequences
 * translated into their meaning.
 */
#include <ctype.h>

int is_fmt(const char c) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\f') {
        return 1;
    } else {
        return 0;
    }
}

char cstrlit_chr(const char *str, const char *end, char const ** const eptr) {
    unsigned char u;
    char rv;

    if (str >= end) {                   /* String contains no data */
        rv = -1;
    } else if ((u = *str++) != '\\') {  /* No need to unescape */
        rv = u;
    } else if (str == end) {            /* Just a backslash '\' - invalid */
        rv = -1;
        EM_error(charPos, "Incomplete escape character");
    } else if (isdigit(u = *str++)) {   /* Ascii character /ddd */
        int d1 = u, d2, d3;

        if (str == end) {  /* Only one digit */
            rv = d1;
        } else if (!isdigit(d2 = *str++)) {
            rv = d1;
            --str;
        } else if (str == end) {  /* Only two digits */
            rv = d1 * 10 + d2;
        } else if (!isdigit(d3 = *str++)) {
            rv = d1 * 10 + d2;
            --str;
        } else if (d1 * 100 + d2 * 10 + d3 > 255) {  /* Out of range */
            rv = -1;
            EM_error(charPos, "ASCII out of range");
        } else {
            rv = d1 * 100 + d2 * 10 + d3;
        }
    } else if (is_fmt(u)) {
        while (is_fmt(u = *str++));
        if (u != '\\') {
            rv = -1;
            EM_error(charPos, "Expecting '\\'");
        } else {
            const char *next;
            rv = cstrlit_chr(str, end, &next);
            str = next;
        }
    } else if (u == '^') {              /* control character */
        if (str == end) {
            rv = -1;    /* '\^' - invalid */
            EM_error(charPos, "Incomplete control character");
        }
        u = toupper(*str++);

        if (u >= 'A' && u <= 'Z') {
            rv = u - 'A' + 1;
        } else {
            switch (u) {
                case '[':
                    rv = '\x1b';
                    break;
                case '\\':
                    rv = '\x1c';
                    break;
                case ']':
                    rv = '\x1d';
                    break;
                case '^':
                    rv = '\x1e';
                    break;
                case '_':
                    rv = '\x1f';
                    break;
                default:
                    rv = -1;
                    EM_error(charPos, "Unsupported control character");
                    break;
            }
        }
    } else {                    /* '\n', '\t', '\"', '\\' */
        switch (u) {
            case 'n':
                rv = '\n';
                break;
            case 't':
                rv = '\t';
                break;
            case '\"':
                rv = '\"';
                break;
            case '\\':
                rv = '\\';
                break;
            default:
                rv = -1;
                EM_error(charPos, "Unsupported escape character");
                break;
        }
    }
    if (eptr != 0)
        *eptr = str;
    return(rv);
}

char *getstr(const char *str) {
    size_t len = strlen(str);
    char c, *unescape = (char *)checked_malloc(len + 1), *pos = unescape;

    // escape first
    const char *start = str, *end = str + len, *next;
    while ((c = cstrlit_chr(start, end, &next)) != -1) {
        *pos++ = c;
        start = next;
    }

    // special care for empty string
    char *result;
    len = strlen(unescape) - 2;  // ignore quotatin mark
    if (len == 0) {
        result = (char *)checked_malloc(7);
        strncpy(result, "(null)", 6);  // NOTE: you keng
    } else {
        result = (char *)checked_malloc(len);
        strncpy(result, unescape + 1, len);
    }
    return result;
}

%}

/* Lex Definitions: */
%Start COMMENT STR

%%

<COMMENT>"/*" {adjust(); ++commentDepth;}
<COMMENT>"*/" {adjust(); --commentDepth; if (commentDepth == 0) {BEGIN INITIAL;}}
<COMMENT>.|\n {adjust();}
<INITIAL>"/*" {adjust(); ++commentDepth; BEGIN COMMENT;}
<INITIAL>"," {adjust(); return COMMA;}
<INITIAL>":" {adjust(); return COLON;}
<INITIAL>";" {adjust(); return SEMICOLON;}
<INITIAL>"(" {adjust(); return LPAREN;}
<INITIAL>")" {adjust(); return RPAREN;}
<INITIAL>"[" {adjust(); return LBRACK;}
<INITIAL>"]" {adjust(); return RBRACK;}
<INITIAL>"{" {adjust(); return LBRACE;}
<INITIAL>"}" {adjust(); return RBRACE;}
<INITIAL>"." {adjust(); return DOT;}
<INITIAL>"+" {adjust(); return PLUS;}
<INITIAL>"-" {adjust(); return MINUS;}
<INITIAL>"*" {adjust(); return TIMES;}
<INITIAL>"/" {adjust(); return DIVIDE;}
<INITIAL>"=" {adjust(); return EQ;}
<INITIAL>"<>" {adjust(); return NEQ;}
<INITIAL>"<" {adjust(); return LT;}
<INITIAL>"<=" {adjust(); return LE;}
<INITIAL>">" {adjust(); return GT;}
<INITIAL>">=" {adjust(); return GE;}
<INITIAL>"&" {adjust(); return AND;}
<INITIAL>"|" {adjust(); return OR;}
<INITIAL>":=" {adjust(); return ASSIGN;}
<INITIAL>"array" {adjust(); return ARRAY;}
<INITIAL>"if" {adjust(); return IF;}
<INITIAL>"then" {adjust(); return THEN;}
<INITIAL>"else" {adjust(); return ELSE;}
<INITIAL>"while" {adjust(); return WHILE;}
<INITIAL>"for" {adjust(); return FOR;}
<INITIAL>"to" {adjust(); return TO;}
<INITIAL>"do" {adjust(); return DO;}
<INITIAL>"let" {adjust(); return LET;}
<INITIAL>"in" {adjust(); return IN;}
<INITIAL>"end" {adjust(); return END;}
<INITIAL>"of" {adjust(); return OF;}
<INITIAL>"break" {adjust(); return BREAK;}
<INITIAL>"nil" {adjust(); return NIL;}
<INITIAL>"function" {adjust(); return FUNCTION;}
<INITIAL>"var" {adjust(); return VAR;}
<INITIAL>"type" {adjust(); yylval.sval=String(yytext); return TYPE;}
<INITIAL>[a-zA-Z][a-zA-Z0-9_]* {adjust(); yylval.sval=String(yytext); return ID;}
<INITIAL>\"[^\"]*\" {adjust(); yylval.sval=getstr(yytext); return STRING;}
<INITIAL>[0-9]+ {adjust(); yylval.ival=atoi(yytext);return INT;}
<INITIAL>\ |\t {adjust();}
<INITIAL>"\n" {adjust(); EM_newline();}
<INITIAL>. {adjust(); EM_error(charPos, yytext);}
