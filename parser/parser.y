%{
#include <iostream>
#include <string>
#include <vector>
#include "AST.h"

extern "C" int yylex();
void yyerror(const char* s) { std::cerr << "Error: " << s << std::endl; }

std::unique_ptr<ProgramAST> rootNode;
%}

%code requires {
    #include "AST.h"
    #include <vector>
    #include <string>
}

%union {
    long long num;
    char* str;
    ASTNode* ast;
    std::vector<StmtPtr>* stmt_list;
    std::vector<ExprPtr>* expr_list;
    std::vector<std::string>* param_list;
    BlockAST* block;
}

%token MAKE TASK GIVE CHOOSE OTHERWISE REPEAT FROM UNTIL WHILE EMIT AND OR NOT
%token <str> IDENTIFIER STRING_LITERAL
%token <num> INTEGER_LITERAL
%token ASSIGN EQUALS NEQ LESS LEQ GREATER GEQ PLUS MINUS STAR SLASH MOD
%token LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE SEMICOLON COMMA

%type <ast> program statement expr var_decl array_decl assignment if_stmt while_stmt range_loop return_stmt emit_stmt func_decl func_call
%type <stmt_list> statement_list
%type <expr_list> expr_list expr_list_opt
%type <param_list> param_list param_list_opt
%type <block> block

%left OR
%left AND
%left EQUALS NEQ LESS LEQ GREATER GEQ
%left PLUS MINUS
%left STAR SLASH MOD
%right NOT

%start start_rule

%%
start_rule:
    program { rootNode = std::unique_ptr<ProgramAST>(static_cast<ProgramAST*>($1)); }
    ;

program:
    statement_list { $$ = new ProgramAST(std::move(*$1)); delete $1; }
    | /* empty */ { $$ = new ProgramAST(std::vector<StmtPtr>()); }
    ;

statement_list:
    statement { $$ = new std::vector<StmtPtr>(); $$->push_back(StmtPtr($1)); }
    | statement_list statement { $$ = $1; $$->push_back(StmtPtr($2)); }
    ;

statement:
    var_decl SEMICOLON { $$ = $1; }
    | array_decl SEMICOLON { $$ = $1; }
    | assignment SEMICOLON { $$ = $1; }
    | if_stmt { $$ = $1; }
    | while_stmt { $$ = $1; }
    | range_loop { $$ = $1; }
    | return_stmt SEMICOLON { $$ = $1; }
    | emit_stmt SEMICOLON { $$ = $1; }
    | func_decl { $$ = $1; }
    | func_call SEMICOLON { $$ = $1; }
    ;

block:
    LBRACE statement_list RBRACE { $$ = new BlockAST(std::move(*$2)); delete $2; }
    | LBRACE RBRACE { $$ = new BlockAST(std::vector<StmtPtr>()); }
    ;

var_decl:
    MAKE IDENTIFIER ASSIGN expr { $$ = new VarDeclAST($2, ExprPtr($4)); free($2); }
    ;

array_decl:
    MAKE IDENTIFIER LBRACKET expr RBRACKET ASSIGN LBRACKET expr_list_opt RBRACKET {
        $$ = new ArrayDeclAST($2, ExprPtr($4), std::move(*$8));
        free($2);
        delete $8;
    }
    ;

assignment:
    IDENTIFIER ASSIGN expr { $$ = new AssignmentAST($1, nullptr, ExprPtr($3)); free($1); }
    | IDENTIFIER LBRACKET expr RBRACKET ASSIGN expr { $$ = new AssignmentAST($1, ExprPtr($3), ExprPtr($6)); free($1); }
    ;

if_stmt:
    CHOOSE expr block { $$ = new IfAST(ExprPtr($2), std::unique_ptr<BlockAST>($3), nullptr); }
    | CHOOSE expr block OTHERWISE block { $$ = new IfAST(ExprPtr($2), std::unique_ptr<BlockAST>($3), std::unique_ptr<BlockAST>($5)); }
    ;

while_stmt:
    REPEAT WHILE expr block { $$ = new WhileAST(ExprPtr($3), std::unique_ptr<BlockAST>($4)); }
    ;

range_loop:
    REPEAT IDENTIFIER FROM expr UNTIL expr block { $$ = new RangeLoopAST($2, ExprPtr($4), ExprPtr($6), std::unique_ptr<BlockAST>($7)); free($2); }
    ;

return_stmt:
    GIVE expr { $$ = new ReturnAST(ExprPtr($2)); }
    ;

emit_stmt:
    EMIT expr { $$ = new EmitAST(ExprPtr($2)); }
    ;

func_decl:
    TASK IDENTIFIER LPAREN param_list_opt RPAREN block {
        $$ = new FunctionAST($2, std::move(*$4), std::unique_ptr<BlockAST>($6));
        free($2);
        delete $4;
    }
    ;

param_list_opt:
    param_list { $$ = $1; }
    | /* empty */ { $$ = new std::vector<std::string>(); }
    ;

param_list:
    IDENTIFIER { $$ = new std::vector<std::string>(); $$->push_back($1); free($1); }
    | param_list COMMA IDENTIFIER { $$ = $1; $$->push_back($3); free($3); }
    ;

expr_list_opt:
    expr_list { $$ = $1; }
    | /* empty */ { $$ = new std::vector<ExprPtr>(); }
    ;

expr_list:
    expr { $$ = new std::vector<ExprPtr>(); $$->push_back(ExprPtr($1)); }
    | expr_list COMMA expr { $$ = $1; $$->push_back(ExprPtr($3)); }
    ;

func_call:
    IDENTIFIER LPAREN expr_list_opt RPAREN {
        $$ = new CallAST($1, std::move(*$3));
        free($1);
        delete $3;
    }
    ;

expr:
    INTEGER_LITERAL { $$ = new NumberExprAST($1); }
    | STRING_LITERAL { $$ = new StringExprAST($1); free($1); }
    | IDENTIFIER { $$ = new VariableExprAST($1); free($1); }
    | IDENTIFIER LBRACKET expr RBRACKET { $$ = new ArrayAccessAST($1, ExprPtr($3)); free($1); }
    | func_call { $$ = $1; }
    | LPAREN expr RPAREN { $$ = $2; }
    | expr PLUS expr { $$ = new BinaryExprAST("+", ExprPtr($1), ExprPtr($3)); }
    | expr MINUS expr { $$ = new BinaryExprAST("-", ExprPtr($1), ExprPtr($3)); }
    | expr STAR expr { $$ = new BinaryExprAST("*", ExprPtr($1), ExprPtr($3)); }
    | expr SLASH expr { $$ = new BinaryExprAST("/", ExprPtr($1), ExprPtr($3)); }
    | expr MOD expr { $$ = new BinaryExprAST("%", ExprPtr($1), ExprPtr($3)); }
    | expr EQUALS expr { $$ = new BinaryExprAST("==", ExprPtr($1), ExprPtr($3)); }
    | expr NEQ expr { $$ = new BinaryExprAST("!=", ExprPtr($1), ExprPtr($3)); }
    | expr LESS expr { $$ = new BinaryExprAST("<", ExprPtr($1), ExprPtr($3)); }
    | expr LEQ expr { $$ = new BinaryExprAST("<=", ExprPtr($1), ExprPtr($3)); }
    | expr GREATER expr { $$ = new BinaryExprAST(">", ExprPtr($1), ExprPtr($3)); }
    | expr GEQ expr { $$ = new BinaryExprAST(">=", ExprPtr($1), ExprPtr($3)); }
    | expr AND expr { $$ = new BinaryExprAST("and", ExprPtr($1), ExprPtr($3)); }
    | expr OR expr { $$ = new BinaryExprAST("or", ExprPtr($1), ExprPtr($3)); }
    | NOT expr { $$ = new UnaryExprAST("not", ExprPtr($2)); }
    | MINUS expr %prec NOT { $$ = new UnaryExprAST("-", ExprPtr($2)); }
    ;
%%
