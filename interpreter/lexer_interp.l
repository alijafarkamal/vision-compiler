%{
/*
 * EdgeFlow Interpreter Lexer — Phase 1
 * Identical token rules to the main lexer, but generates
 * headers for the interpreter parser (interp.tab.hpp).
 */
#include "interp.tab.hpp"
#include <iostream>
#include <cstdlib>
#include <cstring>
extern "C" int yylex();
%}

%option noyywrap
%option yylineno

%%
"//"    {
            int c;
            while ((c = yyinput()) != '\n' && c != EOF);
        }

"/*"    {
            int c;
            while ((c = yyinput()) != EOF) {
                if (c == '*') {
                    if ((c = yyinput()) == '/') break;
                    else unput(c);
                }
            }
        }

"make"       { return MAKE; }
"frame"      { return MAKE; }
"camera"     { return MAKE; }
"model"      { return MAKE; }
"detections" { return MAKE; }
"task"       { return TASK; }
"give"       { return GIVE; }
"choose"     { return CHOOSE; }
"otherwise"  { return OTHERWISE; }
"repeat"     { return REPEAT; }
"from"       { return FROM; }
"until"      { return UNTIL; }
"while"      { return WHILE; }
"emit"       { return EMIT; }
"and"        { return AND; }
"or"         { return OR; }
"not"        { return NOT; }

":="         { return ASSIGN; }
"=="         { return EQUALS; }
"!="         { return NEQ; }
"<="         { return LEQ; }
">="         { return GEQ; }
"<"          { return LESS; }
">"          { return GREATER; }
"+"          { return PLUS; }
"-"          { return MINUS; }
"*"          { return STAR; }
"/"          { return SLASH; }
"%"          { return MOD; }

"("          { return LPAREN; }
")"          { return RPAREN; }
"["          { return LBRACKET; }
"]"          { return RBRACKET; }
"{"          { return LBRACE; }
"}"          { return RBRACE; }
";"          { return SEMICOLON; }
","          { return COMMA; }

[0-9]+       {
                 yylval.num = std::stoll(yytext);
                 return INTEGER_LITERAL;
             }

\"(\\.|[^"\\])*\" {
                 yylval.str = strdup(yytext);
                 return STRING_LITERAL;
             }

[a-zA-Z_][a-zA-Z0-9_]* {
                 yylval.str = strdup(yytext);
                 return IDENTIFIER;
             }

[ \t\n\r]+   { /* ignore whitespace */ }

.            {
                 std::cerr << "Unknown token at line " << yylineno
                           << ": " << yytext << std::endl;
             }
%%
