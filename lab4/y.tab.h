/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ID = 258,
     STRING = 259,
     INT = 260,
     COMMA = 261,
     COLON = 262,
     SEMICOLON = 263,
     LPAREN = 264,
     RPAREN = 265,
     LBRACK = 266,
     RBRACK = 267,
     LBRACE = 268,
     RBRACE = 269,
     DOT = 270,
     ARRAY = 271,
     IF = 272,
     THEN = 273,
     ELSE = 274,
     WHILE = 275,
     FOR = 276,
     TO = 277,
     DO = 278,
     LET = 279,
     IN = 280,
     END = 281,
     OF = 282,
     BREAK = 283,
     NIL = 284,
     FUNCTION = 285,
     VAR = 286,
     TYPE = 287,
     ASSIGN = 288,
     OR = 289,
     AND = 290,
     GE = 291,
     GT = 292,
     LE = 293,
     LT = 294,
     NEQ = 295,
     EQ = 296,
     MINUS = 297,
     PLUS = 298,
     UMINUS = 299,
     DIVIDE = 300,
     TIMES = 301
   };
#endif
/* Tokens.  */
#define ID 258
#define STRING 259
#define INT 260
#define COMMA 261
#define COLON 262
#define SEMICOLON 263
#define LPAREN 264
#define RPAREN 265
#define LBRACK 266
#define RBRACK 267
#define LBRACE 268
#define RBRACE 269
#define DOT 270
#define ARRAY 271
#define IF 272
#define THEN 273
#define ELSE 274
#define WHILE 275
#define FOR 276
#define TO 277
#define DO 278
#define LET 279
#define IN 280
#define END 281
#define OF 282
#define BREAK 283
#define NIL 284
#define FUNCTION 285
#define VAR 286
#define TYPE 287
#define ASSIGN 288
#define OR 289
#define AND 290
#define GE 291
#define GT 292
#define LE 293
#define LT 294
#define NEQ 295
#define EQ 296
#define MINUS 297
#define PLUS 298
#define UMINUS 299
#define DIVIDE 300
#define TIMES 301




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 18 "tiger.y"
{
	int pos;
	int ival;
	string sval;
	A_exp exp;
	A_expList explist;
	A_var var;
	A_decList declist;
	A_dec  dec;
	A_efieldList efieldlist;
	A_efield  efield;
	A_namety namety;
	A_nametyList nametylist;
	A_fieldList fieldlist;
	A_field field;
	A_fundecList fundeclist;
	A_fundec fundec;
	A_ty ty;
}
/* Line 1529 of yacc.c.  */
#line 161 "y.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

