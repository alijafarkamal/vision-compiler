%{
/*
 * EdgeFlow Interpreter — Phase 1
 * Pure Flex + Bison interpreter.
 * No LLVM, no intermediate code generation.
 */
#include <iostream>
#include <cstdlib>
#include <cstring>
extern "C" int yylex();
extern int yylineno;
void yyerror(const char* s) {
    std::cerr << "Error (line " << yylineno << "): " << s << std::endl;
    exit(1);
}
%}

/* =========================================================
 * %code requires — copied into BOTH interp.tab.hpp AND
 * interp.tab.cpp (before the prologue in the header).
 * Put only TYPE DEFINITIONS here so the Flex lexer can see
 * them when it #includes interp.tab.hpp.
 * ========================================================= */
%code requires {
    #include <string>
    #include <vector>
    #include <memory>
    #include <map>

    /* Runtime scalar type */
    using Value = long long;

    /* ---- Expression node kinds ---- */
    enum class EK { Num, Str, Var, ArrayAccess, BinOp, UnOp, Call };

    struct Expr {
        EK          kind;
        Value       num_val = 0;
        std::string str_val;
        std::string op;
        std::shared_ptr<Expr> lhs, rhs, index_expr;
        std::vector<std::shared_ptr<Expr>> args;
    };
    using ExprP = std::shared_ptr<Expr>;

    /* ---- Statement node kinds ---- */
    enum class SK {
        VarDecl, ArrayDecl, Assign, ArrayAssign,
        If, While, RangeFor, Return, Emit, FuncDecl, ExprStmt
    };

    struct Stmt {
        SK          kind;
        std::string name;
        std::shared_ptr<Expr>  expr;
        std::shared_ptr<Expr>  start_e, end_e;
        std::shared_ptr<Expr>  index_expr;
        std::vector<std::shared_ptr<Stmt>> body;
        std::vector<std::shared_ptr<Stmt>> else_body;
        std::vector<std::string>           params;
        std::vector<std::shared_ptr<Expr>> init_exprs;
        std::shared_ptr<Expr>  arr_size;
    };
    using StmtP = std::shared_ptr<Stmt>;
}

/* =========================================================
 * %code — placed AFTER the union/type definitions in the
 * generated interp.tab.cpp, so Value/Expr/Stmt ARE visible.
 * Put all interpreter implementation here.
 * ========================================================= */
%code {
    #include <stdexcept>
    #include <functional>
    #include <cassert>

    /* ---- Return-value signalling ---- */
    struct ReturnSignal { Value value; };

    /* ---- Runtime environment ---- */
    struct Env {
        std::map<std::string, Value>              vars;
        std::map<std::string, std::vector<Value>> arrays;
    };

    struct FuncDef {
        std::vector<std::string> params;
        std::vector<StmtP>       body;
    };

    std::map<std::string, FuncDef> g_functions;
    Env g_env;

    /* ---- Forward declarations ---- */
    Value evalExpr(const ExprP& e, Env& env);
    void  execStmt(const StmtP& s, Env& env);
    void  execBlock(const std::vector<StmtP>& stmts, Env& env);

    /* ---- Array/variable resolution ---- */
    Value& resolveVar(const std::string& name, Env& env) {
        if (env.vars.count(name))   return env.vars[name];
        if (g_env.vars.count(name)) return g_env.vars[name];
        std::cerr << "Error: undefined variable '" << name << "'\n"; exit(1);
    }
    std::vector<Value>& resolveArray(const std::string& name, Env& env) {
        if (env.arrays.count(name))   return env.arrays[name];
        if (g_env.arrays.count(name)) return g_env.arrays[name];
        std::cerr << "Error: undefined array '" << name << "'\n"; exit(1);
    }

    /* ---- Built-in vision stubs ---- */
    Value callBuiltin(const std::string& name, const std::vector<Value>&) {
        static const char* vision[] = {
            "load_image","resize","grayscale","blur","save","show",
            "open_camera","camera_read","load_model","detect", nullptr
        };
        for (int i = 0; vision[i]; ++i)
            if (name == vision[i]) {
                std::cerr << "[interp] vision stub: " << name << "\n";
                return 0;
            }
        std::cerr << "Error: undefined function '" << name << "'\n"; exit(1);
    }

    /* ---- Expression evaluator ---- */
    Value evalExpr(const ExprP& e, Env& env) {
        switch (e->kind) {
        case EK::Num: return e->num_val;
        case EK::Str: return 0;
        case EK::Var: {
            const std::string& n = e->str_val;
            if (env.vars.count(n))   return env.vars[n];
            if (g_env.vars.count(n)) return g_env.vars[n];
            std::cerr << "Error: undefined variable '" << n << "'\n"; exit(1);
        }
        case EK::ArrayAccess: {
            auto& arr = resolveArray(e->str_val, env);
            Value idx = evalExpr(e->index_expr, env);
            if (idx < 0 || idx >= (Value)arr.size()) {
                std::cerr << "Error: index " << idx << " out of bounds\n"; exit(1);
            }
            return arr[(size_t)idx];
        }
        case EK::BinOp: {
            if (e->op == "and") { Value l=evalExpr(e->lhs,env); return l ? (evalExpr(e->rhs,env)?1:0) : 0; }
            if (e->op == "or")  { Value l=evalExpr(e->lhs,env); return l ? 1 : (evalExpr(e->rhs,env)?1:0); }
            Value l=evalExpr(e->lhs,env), r=evalExpr(e->rhs,env);
            if (e->op=="+")  return l+r;
            if (e->op=="-")  return l-r;
            if (e->op=="*")  return l*r;
            if (e->op=="/")  { if(!r){std::cerr<<"div by zero\n";exit(1);} return l/r; }
            if (e->op=="%")  { if(!r){std::cerr<<"mod by zero\n";exit(1);} return l%r; }
            if (e->op=="==") return l==r?1:0;
            if (e->op=="!=") return l!=r?1:0;
            if (e->op=="<")  return l<r?1:0;
            if (e->op=="<=") return l<=r?1:0;
            if (e->op==">")  return l>r?1:0;
            if (e->op==">=") return l>=r?1:0;
            std::cerr<<"unknown op '"<<e->op<<"'\n"; exit(1);
        }
        case EK::UnOp: {
            Value v=evalExpr(e->lhs,env);
            if (e->op=="-")   return -v;
            if (e->op=="not") return v?0:1;
            std::cerr<<"unknown unary op\n"; exit(1);
        }
        case EK::Call: {
            const std::string& fn = e->str_val;
            std::vector<Value> av;
            for (auto& a : e->args) av.push_back(evalExpr(a, env));

            if (fn == "size") {
                if (!e->args.empty() && e->args[0]->kind == EK::Var) {
                    const std::string& an = e->args[0]->str_val;
                    if (env.arrays.count(an))   return (Value)env.arrays[an].size();
                    if (g_env.arrays.count(an)) return (Value)g_env.arrays[an].size();
                }
                return av.empty() ? 0 : av[0];
            }
            if (g_functions.count(fn)) {
                FuncDef& fd = g_functions[fn];
                if (av.size() != fd.params.size()) {
                    std::cerr<<"Error: "<<fn<<" expects "<<fd.params.size()<<" args, got "<<av.size()<<"\n"; exit(1);
                }
                Env local;
                for (size_t i=0;i<fd.params.size();++i) local.vars[fd.params[i]]=av[i];
                try { execBlock(fd.body, local); } catch (ReturnSignal& rs) { return rs.value; }
                return 0;
            }
            return callBuiltin(fn, av);
        }
        }
        return 0;
    }

    /* ---- Statement executor ---- */
    void execBlock(const std::vector<StmtP>& stmts, Env& env) {
        for (auto& s : stmts) execStmt(s, env);
    }

    void execStmt(const StmtP& s, Env& env) {
        switch (s->kind) {
        case SK::VarDecl:
            env.vars[s->name] = s->expr ? evalExpr(s->expr,env) : 0;
            break;
        case SK::ArrayDecl: {
            Value sz = s->arr_size ? evalExpr(s->arr_size,env) : (Value)s->init_exprs.size();
            std::vector<Value> arr((size_t)sz, 0);
            for (size_t i=0;i<s->init_exprs.size()&&i<(size_t)sz;++i)
                arr[i]=evalExpr(s->init_exprs[i],env);
            env.arrays[s->name]=std::move(arr);
            break;
        }
        case SK::Assign: {
            Value v = evalExpr(s->expr,env);
            if (env.vars.count(s->name))   env.vars[s->name]=v;
            else if (g_env.vars.count(s->name)) g_env.vars[s->name]=v;
            else env.vars[s->name]=v;
            break;
        }
        case SK::ArrayAssign: {
            Value idx=evalExpr(s->index_expr,env), val=evalExpr(s->expr,env);
            auto& arr=resolveArray(s->name,env);
            if (idx<0||idx>=(Value)arr.size()){std::cerr<<"index out of bounds\n";exit(1);}
            arr[(size_t)idx]=val;
            break;
        }
        case SK::If:
            if (evalExpr(s->expr,env)) execBlock(s->body,env);
            else if (!s->else_body.empty()) execBlock(s->else_body,env);
            break;
        case SK::While:
            while (evalExpr(s->expr,env)) execBlock(s->body,env);
            break;
        case SK::RangeFor: {
            Value from=evalExpr(s->start_e,env), to=evalExpr(s->end_e,env);
            for (Value i=from;i<to;++i) { env.vars[s->name]=i; execBlock(s->body,env); }
            env.vars.erase(s->name);
            break;
        }
        case SK::Return:
            throw ReturnSignal{s->expr ? evalExpr(s->expr,env) : 0};
        case SK::Emit:
            std::cout << evalExpr(s->expr,env) << "\n";
            break;
        case SK::FuncDecl: {
            FuncDef fd; fd.params=s->params; fd.body=s->body;
            g_functions[s->name]=std::move(fd);
            break;
        }
        case SK::ExprStmt:
            evalExpr(s->expr,env);
            break;
        }
    }

    /* ---- Node builder helpers ---- */
    ExprP mkNum(Value v)       { auto e=std::make_shared<Expr>(); e->kind=EK::Num; e->num_val=v; return e; }
    ExprP mkStr(const char* s) { auto e=std::make_shared<Expr>(); e->kind=EK::Str; e->str_val=s; return e; }
    ExprP mkVar(const char* n) { auto e=std::make_shared<Expr>(); e->kind=EK::Var; e->str_val=n; return e; }
    ExprP mkAcc(const char* n, ExprP idx) {
        auto e=std::make_shared<Expr>(); e->kind=EK::ArrayAccess; e->str_val=n; e->index_expr=idx; return e;
    }
    ExprP mkBin(const char* op, ExprP l, ExprP r) {
        auto e=std::make_shared<Expr>(); e->kind=EK::BinOp; e->op=op; e->lhs=l; e->rhs=r; return e;
    }
    ExprP mkUn(const char* op, ExprP x) {
        auto e=std::make_shared<Expr>(); e->kind=EK::UnOp; e->op=op; e->lhs=x; return e;
    }
    ExprP mkCall(const char* n, std::vector<ExprP> av) {
        auto e=std::make_shared<Expr>(); e->kind=EK::Call; e->str_val=n; e->args=std::move(av); return e;
    }

    StmtP mkVarDecl(const char* n, ExprP init) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::VarDecl; s->name=n; s->expr=init; return s;
    }
    StmtP mkArrDecl(const char* n, ExprP sz, std::vector<ExprP> elems) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::ArrayDecl; s->name=n; s->arr_size=sz; s->init_exprs=std::move(elems); return s;
    }
    StmtP mkAssign(const char* n, ExprP v) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::Assign; s->name=n; s->expr=v; return s;
    }
    StmtP mkArrAssign(const char* n, ExprP idx, ExprP v) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::ArrayAssign; s->name=n; s->index_expr=idx; s->expr=v; return s;
    }
    StmtP mkIf(ExprP c, std::vector<StmtP> th, std::vector<StmtP> el) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::If; s->expr=c; s->body=std::move(th); s->else_body=std::move(el); return s;
    }
    StmtP mkWhile(ExprP c, std::vector<StmtP> b) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::While; s->expr=c; s->body=std::move(b); return s;
    }
    StmtP mkFor(const char* v, ExprP from, ExprP to, std::vector<StmtP> b) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::RangeFor; s->name=v; s->start_e=from; s->end_e=to; s->body=std::move(b); return s;
    }
    StmtP mkReturn(ExprP v) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::Return; s->expr=v; return s;
    }
    StmtP mkEmit(ExprP v) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::Emit; s->expr=v; return s;
    }
    StmtP mkFuncDecl(const char* n, std::vector<std::string> p, std::vector<StmtP> b) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::FuncDecl; s->name=n; s->params=std::move(p); s->body=std::move(b); return s;
    }
    StmtP mkExprStmt(ExprP e) {
        auto s=std::make_shared<Stmt>(); s->kind=SK::ExprStmt; s->expr=e; return s;
    }
}

/* =========================================================
 * BISON TOKEN & TYPE DECLARATIONS
 * ========================================================= */
%union {
    long long                            num;
    char*                                str;
    std::shared_ptr<Expr>*               expr;
    std::shared_ptr<Stmt>*               stmt;
    std::vector<std::shared_ptr<Stmt>>*  stmts;
    std::vector<std::shared_ptr<Expr>>*  exprs;
    std::vector<std::string>*            params;
}

%token MAKE TASK GIVE CHOOSE OTHERWISE REPEAT FROM UNTIL WHILE EMIT AND OR NOT
%token <str> IDENTIFIER STRING_LITERAL
%token <num> INTEGER_LITERAL
%token ASSIGN EQUALS NEQ LESS LEQ GREATER GEQ PLUS MINUS STAR SLASH MOD
%token LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE SEMICOLON COMMA

%type <expr>   expr func_call_expr
%type <stmt>   statement var_decl array_decl assignment if_stmt while_stmt
%type <stmt>   range_loop return_stmt emit_stmt func_decl func_call_stmt
%type <stmts>  statement_list block
%type <exprs>  expr_list expr_list_opt
%type <params> param_list param_list_opt

%left OR
%left AND
%left EQUALS NEQ LESS LEQ GREATER GEQ
%left PLUS MINUS
%left STAR SLASH MOD
%right NOT UMINUS

%start program
%%

/* =========================================================
 * GRAMMAR
 * ========================================================= */

program:
    statement_list {
        for (auto& s : *$1) execStmt(s, g_env);
        delete $1;
    }
    | /* empty */
    ;

statement_list:
    statement {
        $$ = new std::vector<StmtP>();
        if ($1) { $$->push_back(*$1); delete $1; }
    }
    | statement_list statement {
        $$ = $1;
        if ($2) { $$->push_back(*$2); delete $2; }
    }
    ;

block:
    LBRACE statement_list RBRACE { $$ = $2; }
    | LBRACE RBRACE              { $$ = new std::vector<StmtP>(); }
    ;

statement:
    var_decl         SEMICOLON { $$ = $1; }
    | array_decl     SEMICOLON { $$ = $1; }
    | assignment     SEMICOLON { $$ = $1; }
    | if_stmt                  { $$ = $1; }
    | while_stmt               { $$ = $1; }
    | range_loop               { $$ = $1; }
    | return_stmt    SEMICOLON { $$ = $1; }
    | emit_stmt      SEMICOLON { $$ = $1; }
    | func_decl                { $$ = $1; }
    | func_call_stmt SEMICOLON { $$ = $1; }
    ;

var_decl:
    MAKE IDENTIFIER ASSIGN expr {
        $$ = new StmtP(mkVarDecl($2, *$4));
        free($2); delete $4;
    }
    ;

array_decl:
    MAKE IDENTIFIER LBRACKET expr RBRACKET ASSIGN LBRACKET expr_list_opt RBRACKET {
        $$ = new StmtP(mkArrDecl($2, *$4, std::move(*$8)));
        free($2); delete $4; delete $8;
    }
    ;

assignment:
    IDENTIFIER ASSIGN expr {
        $$ = new StmtP(mkAssign($1, *$3));
        free($1); delete $3;
    }
    | IDENTIFIER LBRACKET expr RBRACKET ASSIGN expr {
        $$ = new StmtP(mkArrAssign($1, *$3, *$6));
        free($1); delete $3; delete $6;
    }
    ;

if_stmt:
    CHOOSE expr block {
        $$ = new StmtP(mkIf(*$2, *$3, {}));
        delete $2; delete $3;
    }
    | CHOOSE expr block OTHERWISE block {
        $$ = new StmtP(mkIf(*$2, *$3, *$5));
        delete $2; delete $3; delete $5;
    }
    ;

while_stmt:
    REPEAT WHILE expr block {
        $$ = new StmtP(mkWhile(*$3, *$4));
        delete $3; delete $4;
    }
    ;

range_loop:
    REPEAT IDENTIFIER FROM expr UNTIL expr block {
        $$ = new StmtP(mkFor($2, *$4, *$6, *$7));
        free($2); delete $4; delete $6; delete $7;
    }
    ;

return_stmt:
    GIVE expr {
        $$ = new StmtP(mkReturn(*$2));
        delete $2;
    }
    ;

emit_stmt:
    EMIT expr {
        $$ = new StmtP(mkEmit(*$2));
        delete $2;
    }
    ;

func_decl:
    TASK IDENTIFIER LPAREN param_list_opt RPAREN block {
        $$ = new StmtP(mkFuncDecl($2, *$4, *$6));
        free($2); delete $4; delete $6;
    }
    ;

func_call_stmt:
    IDENTIFIER LPAREN expr_list_opt RPAREN {
        $$ = new StmtP(mkExprStmt(mkCall($1, std::move(*$3))));
        free($1); delete $3;
    }
    ;

param_list_opt:
    param_list    { $$ = $1; }
    | /* empty */ { $$ = new std::vector<std::string>(); }
    ;

param_list:
    IDENTIFIER {
        $$ = new std::vector<std::string>();
        $$->push_back($1); free($1);
    }
    | param_list COMMA IDENTIFIER {
        $$ = $1; $$->push_back($3); free($3);
    }
    ;

expr_list_opt:
    expr_list     { $$ = $1; }
    | /* empty */ { $$ = new std::vector<ExprP>(); }
    ;

expr_list:
    expr {
        $$ = new std::vector<ExprP>();
        $$->push_back(*$1); delete $1;
    }
    | expr_list COMMA expr {
        $$ = $1; $$->push_back(*$3); delete $3;
    }
    ;

func_call_expr:
    IDENTIFIER LPAREN expr_list_opt RPAREN {
        $$ = new ExprP(mkCall($1, std::move(*$3)));
        free($1); delete $3;
    }
    ;

expr:
    INTEGER_LITERAL              { $$ = new ExprP(mkNum($1)); }
    | STRING_LITERAL             { $$ = new ExprP(mkStr($1)); free($1); }
    | IDENTIFIER                 { $$ = new ExprP(mkVar($1)); free($1); }
    | IDENTIFIER LBRACKET expr RBRACKET {
        $$ = new ExprP(mkAcc($1, *$3)); free($1); delete $3;
    }
    | func_call_expr             { $$ = $1; }
    | LPAREN expr RPAREN         { $$ = $2; }
    | expr PLUS   expr  { $$ = new ExprP(mkBin("+",  *$1,*$3)); delete $1; delete $3; }
    | expr MINUS  expr  { $$ = new ExprP(mkBin("-",  *$1,*$3)); delete $1; delete $3; }
    | expr STAR   expr  { $$ = new ExprP(mkBin("*",  *$1,*$3)); delete $1; delete $3; }
    | expr SLASH  expr  { $$ = new ExprP(mkBin("/",  *$1,*$3)); delete $1; delete $3; }
    | expr MOD    expr  { $$ = new ExprP(mkBin("%",  *$1,*$3)); delete $1; delete $3; }
    | expr EQUALS  expr { $$ = new ExprP(mkBin("==", *$1,*$3)); delete $1; delete $3; }
    | expr NEQ     expr { $$ = new ExprP(mkBin("!=", *$1,*$3)); delete $1; delete $3; }
    | expr LESS    expr { $$ = new ExprP(mkBin("<",  *$1,*$3)); delete $1; delete $3; }
    | expr LEQ     expr { $$ = new ExprP(mkBin("<=", *$1,*$3)); delete $1; delete $3; }
    | expr GREATER expr { $$ = new ExprP(mkBin(">",  *$1,*$3)); delete $1; delete $3; }
    | expr GEQ     expr { $$ = new ExprP(mkBin(">=", *$1,*$3)); delete $1; delete $3; }
    | expr AND     expr { $$ = new ExprP(mkBin("and",*$1,*$3)); delete $1; delete $3; }
    | expr OR      expr { $$ = new ExprP(mkBin("or", *$1,*$3)); delete $1; delete $3; }
    | NOT   expr        { $$ = new ExprP(mkUn("not", *$2)); delete $2; }
    | MINUS expr %prec UMINUS { $$ = new ExprP(mkUn("-", *$2)); delete $2; }
    ;

%%
