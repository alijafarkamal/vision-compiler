%{
/*
 * EdgeFlow Interpreter — Phase 1
 * Pure Flex + Bison interpreter.
 * Grammar actions directly execute the program using C++ data structures.
 * No LLVM, no intermediate code generation.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <cassert>

extern "C" int yylex();
extern int yylineno;
void yyerror(const char* s) {
    std::cerr << "Error (line " << yylineno << "): " << s << std::endl;
    exit(1);
}

/* =========================================================
 *  SELF-CONTAINED VALUE / EXPRESSION / STATEMENT TYPES
 *  Everything is defined right here in the .y file.
 * ========================================================= */

/* A runtime value — all scalars are 64-bit integers */
using Value = long long;

/* ---- Expr kinds ---- */
enum class EK {
    Num, Str, Var, ArrayAccess,
    BinOp, UnOp, Call
};

struct Expr {
    EK kind;
    Value          num_val   = 0;
    std::string    str_val;          // variable name, op string, or string literal
    std::string    op;
    std::shared_ptr<Expr> lhs, rhs, index_expr;
    std::vector<std::shared_ptr<Expr>> args;   // for Call
};

using ExprP = std::shared_ptr<Expr>;

/* ---- Stmt kinds ---- */
enum class SK {
    VarDecl, ArrayDecl, Assign, ArrayAssign,
    If, While, RangeFor,
    Return, Emit, FuncDecl, ExprStmt
};

struct Stmt {
    SK kind;
    std::string name;                        // variable / function name
    ExprP       expr;                        // RHS / condition / emit value
    ExprP       start_e, end_e;             // range loop bounds
    ExprP       index_expr;                  // array assignment index
    std::vector<std::shared_ptr<Stmt>> body;       // then-block / loop body / func body
    std::vector<std::shared_ptr<Stmt>> else_body;  // otherwise block
    std::vector<std::string> params;         // function parameters
    std::vector<ExprP>       init_exprs;    // array initializer list
    ExprP                    arr_size;       // array declared size
};

using StmtP = std::shared_ptr<Stmt>;

/* =========================================================
 *  INTERPRETER STATE
 * ========================================================= */

/* Return-value signalling via exception */
struct ReturnSignal {
    Value value;
};

/* A local environment: scalar vars + arrays */
struct Env {
    std::map<std::string, Value>                   vars;
    std::map<std::string, std::vector<Value>>      arrays;
    Env* parent = nullptr;   // for lexical scoping (unused here — we use flat env per call)
};

/* Function definitions */
struct FuncDef {
    std::vector<std::string>        params;
    std::vector<StmtP>              body;
};

/* Global state */
std::map<std::string, FuncDef> g_functions;  // user-defined functions
Env g_env;                                    // global environment

/* =========================================================
 *  FORWARD DECLARATIONS
 * ========================================================= */
Value evalExpr(const ExprP& e, Env& env);
void  execStmt(const StmtP& s, Env& env);
void  execBlock(const std::vector<StmtP>& stmts, Env& env);

/* =========================================================
 *  BUILT-IN VISION FUNCTIONS (stubs — Phase 1 interpreter)
 * ========================================================= */
Value callBuiltin(const std::string& name,
                  const std::vector<Value>& argvals)
{
    if (name == "size") {
        // size(arr) — array size is tracked separately; just return 0 as stub
        std::cerr << "[interp] built-in size() called with scalar arg" << std::endl;
        return 0;
    }
    if (name == "load_image" || name == "resize" || name == "grayscale" ||
        name == "blur"       || name == "save"   || name == "show"      ||
        name == "open_camera"|| name == "camera_read" || name == "load_model" ||
        name == "detect") {
        std::cerr << "[interp] vision built-in '" << name
                  << "' — stub (Phase 1 interpreter)" << std::endl;
        return 0;
    }
    std::cerr << "Error: undefined function '" << name << "'" << std::endl;
    exit(1);
}

/* Resolve variable: look in env, then global */
Value& resolveVar(const std::string& name, Env& env) {
    if (env.vars.count(name)) return env.vars[name];
    if (&env != &g_env && g_env.vars.count(name)) return g_env.vars[name];
    std::cerr << "Error: undefined variable '" << name << "'" << std::endl;
    exit(1);
}

/* Resolve array */
std::vector<Value>& resolveArray(const std::string& name, Env& env) {
    if (env.arrays.count(name)) return env.arrays[name];
    if (&env != &g_env && g_env.arrays.count(name)) return g_env.arrays[name];
    std::cerr << "Error: undefined array '" << name << "'" << std::endl;
    exit(1);
}

/* =========================================================
 *  EXPRESSION EVALUATOR
 * ========================================================= */
Value evalExpr(const ExprP& e, Env& env) {
    switch (e->kind) {
    case EK::Num:
        return e->num_val;

    case EK::Str:
        // String literals evaluate to 0 in numeric context
        return 0;

    case EK::Var: {
        // Could be a scalar variable or an array name passed to size()
        const std::string& n = e->str_val;
        if (env.vars.count(n)) return env.vars[n];
        if (&env != &g_env && g_env.vars.count(n)) return g_env.vars[n];
        std::cerr << "Error: undefined variable '" << n << "'" << std::endl;
        exit(1);
    }

    case EK::ArrayAccess: {
        auto& arr = resolveArray(e->str_val, env);
        Value idx = evalExpr(e->index_expr, env);
        if (idx < 0 || idx >= (Value)arr.size()) {
            std::cerr << "Error: array index " << idx << " out of bounds" << std::endl;
            exit(1);
        }
        return arr[(size_t)idx];
    }

    case EK::BinOp: {
        // Short-circuit for 'and' / 'or'
        if (e->op == "and") {
            Value l = evalExpr(e->lhs, env);
            if (!l) return 0;
            return evalExpr(e->rhs, env) ? 1 : 0;
        }
        if (e->op == "or") {
            Value l = evalExpr(e->lhs, env);
            if (l) return 1;
            return evalExpr(e->rhs, env) ? 1 : 0;
        }
        Value l = evalExpr(e->lhs, env);
        Value r = evalExpr(e->rhs, env);
        if (e->op == "+")  return l + r;
        if (e->op == "-")  return l - r;
        if (e->op == "*")  return l * r;
        if (e->op == "/") {
            if (r == 0) { std::cerr << "Error: division by zero\n"; exit(1); }
            return l / r;
        }
        if (e->op == "%") {
            if (r == 0) { std::cerr << "Error: modulo by zero\n"; exit(1); }
            return l % r;
        }
        if (e->op == "==") return (l == r) ? 1 : 0;
        if (e->op == "!=") return (l != r) ? 1 : 0;
        if (e->op == "<")  return (l <  r) ? 1 : 0;
        if (e->op == "<=") return (l <= r) ? 1 : 0;
        if (e->op == ">")  return (l >  r) ? 1 : 0;
        if (e->op == ">=") return (l >= r) ? 1 : 0;
        std::cerr << "Error: unknown binary operator '" << e->op << "'\n";
        exit(1);
    }

    case EK::UnOp: {
        Value v = evalExpr(e->lhs, env);
        if (e->op == "-")   return -v;
        if (e->op == "not") return v ? 0 : 1;
        std::cerr << "Error: unknown unary operator '" << e->op << "'\n";
        exit(1);
    }

    case EK::Call: {
        const std::string& fname = e->str_val;

        /* Evaluate all arguments */
        std::vector<Value> argvals;
        for (auto& a : e->args)
            argvals.push_back(evalExpr(a, env));

        /* Special built-in: size(arrName) */
        if (fname == "size") {
            /* size takes the array name as first arg — but in our grammar
               it's parsed as an expression; if it was a Var node, look it
               up as an array */
            if (!e->args.empty() && e->args[0]->kind == EK::Var) {
                const std::string& aname = e->args[0]->str_val;
                if (env.arrays.count(aname))
                    return (Value)env.arrays[aname].size();
                if (g_env.arrays.count(aname))
                    return (Value)g_env.arrays[aname].size();
            }
            return argvals.empty() ? 0 : argvals[0]; // fallback
        }

        /* User-defined function */
        if (g_functions.count(fname)) {
            FuncDef& fd = g_functions[fname];
            if (argvals.size() != fd.params.size()) {
                std::cerr << "Error: function '" << fname << "' expects "
                          << fd.params.size() << " arguments, got "
                          << argvals.size() << std::endl;
                exit(1);
            }
            /* Create local environment */
            Env local;
            for (size_t i = 0; i < fd.params.size(); ++i)
                local.vars[fd.params[i]] = argvals[i];

            try {
                execBlock(fd.body, local);
            } catch (ReturnSignal& rs) {
                return rs.value;
            }
            return 0; // void function
        }

        /* Built-in vision stubs */
        return callBuiltin(fname, argvals);
    }
    }
    return 0;
}

/* =========================================================
 *  STATEMENT EXECUTOR
 * ========================================================= */
void execBlock(const std::vector<StmtP>& stmts, Env& env) {
    for (auto& s : stmts) execStmt(s, env);
}

void execStmt(const StmtP& s, Env& env) {
    switch (s->kind) {

    case SK::VarDecl: {
        Value v = s->expr ? evalExpr(s->expr, env) : 0;
        env.vars[s->name] = v;
        break;
    }

    case SK::ArrayDecl: {
        Value sz = s->arr_size ? evalExpr(s->arr_size, env) : (Value)s->init_exprs.size();
        std::vector<Value> arr((size_t)sz, 0);
        for (size_t i = 0; i < s->init_exprs.size() && i < (size_t)sz; ++i)
            arr[i] = evalExpr(s->init_exprs[i], env);
        env.arrays[s->name] = std::move(arr);
        break;
    }

    case SK::Assign: {
        Value v = evalExpr(s->expr, env);
        // Look for variable in current env, then global
        if (env.vars.count(s->name)) {
            env.vars[s->name] = v;
        } else if (&env != &g_env && g_env.vars.count(s->name)) {
            g_env.vars[s->name] = v;
        } else {
            // Create in current env
            env.vars[s->name] = v;
        }
        break;
    }

    case SK::ArrayAssign: {
        Value idx = evalExpr(s->index_expr, env);
        Value val = evalExpr(s->expr, env);
        auto& arr = resolveArray(s->name, env);
        if (idx < 0 || idx >= (Value)arr.size()) {
            std::cerr << "Error: array index " << idx << " out of bounds\n";
            exit(1);
        }
        arr[(size_t)idx] = val;
        break;
    }

    case SK::If: {
        Value cond = evalExpr(s->expr, env);
        if (cond) {
            execBlock(s->body, env);
        } else if (!s->else_body.empty()) {
            execBlock(s->else_body, env);
        }
        break;
    }

    case SK::While: {
        while (evalExpr(s->expr, env)) {
            execBlock(s->body, env);
        }
        break;
    }

    case SK::RangeFor: {
        Value from_val = evalExpr(s->start_e, env);
        Value until_val = evalExpr(s->end_e, env);
        env.vars[s->name] = from_val;
        for (Value i = from_val; i < until_val; ++i) {
            env.vars[s->name] = i;
            execBlock(s->body, env);
        }
        // Clean up loop variable
        env.vars.erase(s->name);
        break;
    }

    case SK::Return: {
        Value v = s->expr ? evalExpr(s->expr, env) : 0;
        throw ReturnSignal{v};
    }

    case SK::Emit: {
        Value v = evalExpr(s->expr, env);
        std::cout << v << std::endl;
        break;
    }

    case SK::FuncDecl: {
        FuncDef fd;
        fd.params = s->params;
        fd.body   = s->body;
        g_functions[s->name] = std::move(fd);
        break;
    }

    case SK::ExprStmt: {
        evalExpr(s->expr, env);  // side effects only (e.g. function call)
        break;
    }
    }
}

/* =========================================================
 *  HELPER BUILDERS
 * ========================================================= */
ExprP makeNum(long long v) {
    auto e = std::make_shared<Expr>(); e->kind = EK::Num; e->num_val = v; return e;
}
ExprP makeStr(const std::string& s) {
    auto e = std::make_shared<Expr>(); e->kind = EK::Str; e->str_val = s; return e;
}
ExprP makeVar(const std::string& n) {
    auto e = std::make_shared<Expr>(); e->kind = EK::Var; e->str_val = n; return e;
}
ExprP makeArrAccess(const std::string& n, ExprP idx) {
    auto e = std::make_shared<Expr>(); e->kind = EK::ArrayAccess;
    e->str_val = n; e->index_expr = std::move(idx); return e;
}
ExprP makeBin(const std::string& op, ExprP l, ExprP r) {
    auto e = std::make_shared<Expr>(); e->kind = EK::BinOp;
    e->op = op; e->lhs = std::move(l); e->rhs = std::move(r); return e;
}
ExprP makeUnary(const std::string& op, ExprP operand) {
    auto e = std::make_shared<Expr>(); e->kind = EK::UnOp;
    e->op = op; e->lhs = std::move(operand); return e;
}
ExprP makeCall(const std::string& name, std::vector<ExprP> args) {
    auto e = std::make_shared<Expr>(); e->kind = EK::Call;
    e->str_val = name; e->args = std::move(args); return e;
}

StmtP makeVarDecl(const std::string& name, ExprP init) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::VarDecl;
    s->name = name; s->expr = std::move(init); return s;
}
StmtP makeArrDecl(const std::string& name, ExprP sz, std::vector<ExprP> inits) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::ArrayDecl;
    s->name = name; s->arr_size = std::move(sz); s->init_exprs = std::move(inits); return s;
}
StmtP makeAssign(const std::string& name, ExprP val) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::Assign;
    s->name = name; s->expr = std::move(val); return s;
}
StmtP makeArrAssign(const std::string& name, ExprP idx, ExprP val) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::ArrayAssign;
    s->name = name; s->index_expr = std::move(idx); s->expr = std::move(val); return s;
}
StmtP makeIf(ExprP cond, std::vector<StmtP> then_b, std::vector<StmtP> else_b) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::If;
    s->expr = std::move(cond); s->body = std::move(then_b); s->else_body = std::move(else_b);
    return s;
}
StmtP makeWhile(ExprP cond, std::vector<StmtP> body) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::While;
    s->expr = std::move(cond); s->body = std::move(body); return s;
}
StmtP makeRangeFor(const std::string& var, ExprP from, ExprP to, std::vector<StmtP> body) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::RangeFor;
    s->name = var; s->start_e = std::move(from); s->end_e = std::move(to);
    s->body = std::move(body); return s;
}
StmtP makeReturn(ExprP val) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::Return;
    s->expr = std::move(val); return s;
}
StmtP makeEmit(ExprP val) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::Emit;
    s->expr = std::move(val); return s;
}
StmtP makeFuncDecl(const std::string& name, std::vector<std::string> params, std::vector<StmtP> body) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::FuncDecl;
    s->name = name; s->params = std::move(params); s->body = std::move(body); return s;
}
StmtP makeExprStmt(ExprP e) {
    auto s = std::make_shared<Stmt>(); s->kind = SK::ExprStmt;
    s->expr = std::move(e); return s;
}

%}

/* =========================================================
 *  BISON DECLARATIONS
 * ========================================================= */

%code requires {
    #include <vector>
    #include <string>
    #include <memory>
}

%union {
    long long                            num;
    char*                                str;
    std::shared_ptr<Expr>*               expr;   /* heap-allocated shared_ptr */
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
%type <stmt>   statement var_decl array_decl assignment if_stmt while_stmt range_loop return_stmt emit_stmt func_decl func_call_stmt
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
 *  GRAMMAR RULES WITH DIRECT INTERPRETER ACTIONS
 * ========================================================= */

program:
    statement_list {
        /* Execute all top-level statements immediately */
        for (auto& s : *$1)
            execStmt(s, g_env);
        delete $1;
    }
    | /* empty */
    ;

statement_list:
    statement {
        $$ = new std::vector<std::shared_ptr<Stmt>>();
        if ($1) $$->push_back(*$1), delete $1;
    }
    | statement_list statement {
        $$ = $1;
        if ($2) $$->push_back(*$2), delete $2;
    }
    ;

block:
    LBRACE statement_list RBRACE { $$ = $1; }
    | LBRACE RBRACE              { $$ = new std::vector<std::shared_ptr<Stmt>>(); }
    ;

statement:
    var_decl      SEMICOLON { $$ = $1; }
    | array_decl  SEMICOLON { $$ = $1; }
    | assignment  SEMICOLON { $$ = $1; }
    | if_stmt               { $$ = $1; }
    | while_stmt            { $$ = $1; }
    | range_loop            { $$ = $1; }
    | return_stmt SEMICOLON { $$ = $1; }
    | emit_stmt   SEMICOLON { $$ = $1; }
    | func_decl             { $$ = $1; }
    | func_call_stmt SEMICOLON { $$ = $1; }
    ;

/* ---- Variable declaration: make x := expr ---- */
var_decl:
    MAKE IDENTIFIER ASSIGN expr {
        auto s = makeVarDecl($2, *$4);
        free($2); delete $4;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- Array declaration: make arr[n] := [e1, e2, ...] ---- */
array_decl:
    MAKE IDENTIFIER LBRACKET expr RBRACKET ASSIGN LBRACKET expr_list_opt RBRACKET {
        auto s = makeArrDecl($2, *$4, std::move(*$8));
        free($2); delete $4; delete $8;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- Assignment: x := expr  OR  x[idx] := expr ---- */
assignment:
    IDENTIFIER ASSIGN expr {
        auto s = makeAssign($1, *$3);
        free($1); delete $3;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    | IDENTIFIER LBRACKET expr RBRACKET ASSIGN expr {
        auto s = makeArrAssign($1, *$3, *$6);
        free($1); delete $3; delete $6;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- If/else: choose cond { } otherwise { } ---- */
if_stmt:
    CHOOSE expr block {
        auto s = makeIf(*$2, *$3, {});
        delete $2; delete $3;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    | CHOOSE expr block OTHERWISE block {
        auto s = makeIf(*$2, *$3, *$5);
        delete $2; delete $3; delete $5;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- While loop: repeat while cond { } ---- */
while_stmt:
    REPEAT WHILE expr block {
        auto s = makeWhile(*$3, *$4);
        delete $3; delete $4;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- Range loop: repeat i from start until end { } ---- */
range_loop:
    REPEAT IDENTIFIER FROM expr UNTIL expr block {
        auto s = makeRangeFor($2, *$4, *$6, *$7);
        free($2); delete $4; delete $6; delete $7;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- Return: give expr ---- */
return_stmt:
    GIVE expr {
        auto s = makeReturn(*$2);
        delete $2;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- Emit (print): emit expr ---- */
emit_stmt:
    EMIT expr {
        auto s = makeEmit(*$2);
        delete $2;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- Function declaration: task name(params) { body } ---- */
func_decl:
    TASK IDENTIFIER LPAREN param_list_opt RPAREN block {
        auto s = makeFuncDecl($2, *$4, *$6);
        free($2); delete $4; delete $6;
        $$ = new std::shared_ptr<Stmt>(s);
    }
    ;

/* ---- Standalone function call as statement: foo(args); ---- */
func_call_stmt:
    IDENTIFIER LPAREN expr_list_opt RPAREN {
        auto call_e = makeCall($1, std::move(*$3));
        free($1); delete $3;
        $$ = new std::shared_ptr<Stmt>(makeExprStmt(call_e));
    }
    ;

/* ---- Parameter lists ---- */
param_list_opt:
    param_list   { $$ = $1; }
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

/* ---- Expression argument lists ---- */
expr_list_opt:
    expr_list   { $$ = $1; }
    | /* empty */ { $$ = new std::vector<std::shared_ptr<Expr>>(); }
    ;

expr_list:
    expr {
        $$ = new std::vector<std::shared_ptr<Expr>>();
        $$->push_back(*$1); delete $1;
    }
    | expr_list COMMA expr {
        $$ = $1; $$->push_back(*$3); delete $3;
    }
    ;

/* ---- Expressions ---- */
func_call_expr:
    IDENTIFIER LPAREN expr_list_opt RPAREN {
        auto e = makeCall($1, std::move(*$3));
        free($1); delete $3;
        $$ = new std::shared_ptr<Expr>(e);
    }
    ;

expr:
    INTEGER_LITERAL {
        $$ = new std::shared_ptr<Expr>(makeNum($1));
    }
    | STRING_LITERAL {
        auto e = makeStr($1); free($1);
        $$ = new std::shared_ptr<Expr>(e);
    }
    | IDENTIFIER {
        $$ = new std::shared_ptr<Expr>(makeVar($1)); free($1);
    }
    | IDENTIFIER LBRACKET expr RBRACKET {
        auto e = makeArrAccess($1, *$3);
        free($1); delete $3;
        $$ = new std::shared_ptr<Expr>(e);
    }
    | func_call_expr { $$ = $1; }
    | LPAREN expr RPAREN { $$ = $2; }
    | expr PLUS  expr { $$ = new std::shared_ptr<Expr>(makeBin("+",  *$1, *$3)); delete $1; delete $3; }
    | expr MINUS expr { $$ = new std::shared_ptr<Expr>(makeBin("-",  *$1, *$3)); delete $1; delete $3; }
    | expr STAR  expr { $$ = new std::shared_ptr<Expr>(makeBin("*",  *$1, *$3)); delete $1; delete $3; }
    | expr SLASH expr { $$ = new std::shared_ptr<Expr>(makeBin("/",  *$1, *$3)); delete $1; delete $3; }
    | expr MOD   expr { $$ = new std::shared_ptr<Expr>(makeBin("%",  *$1, *$3)); delete $1; delete $3; }
    | expr EQUALS  expr { $$ = new std::shared_ptr<Expr>(makeBin("==", *$1, *$3)); delete $1; delete $3; }
    | expr NEQ     expr { $$ = new std::shared_ptr<Expr>(makeBin("!=", *$1, *$3)); delete $1; delete $3; }
    | expr LESS    expr { $$ = new std::shared_ptr<Expr>(makeBin("<",  *$1, *$3)); delete $1; delete $3; }
    | expr LEQ     expr { $$ = new std::shared_ptr<Expr>(makeBin("<=", *$1, *$3)); delete $1; delete $3; }
    | expr GREATER expr { $$ = new std::shared_ptr<Expr>(makeBin(">",  *$1, *$3)); delete $1; delete $3; }
    | expr GEQ     expr { $$ = new std::shared_ptr<Expr>(makeBin(">=", *$1, *$3)); delete $1; delete $3; }
    | expr AND     expr { $$ = new std::shared_ptr<Expr>(makeBin("and", *$1, *$3)); delete $1; delete $3; }
    | expr OR      expr { $$ = new std::shared_ptr<Expr>(makeBin("or",  *$1, *$3)); delete $1; delete $3; }
    | NOT expr          { $$ = new std::shared_ptr<Expr>(makeUnary("not", *$2)); delete $2; }
    | MINUS expr %prec UMINUS { $$ = new std::shared_ptr<Expr>(makeUnary("-", *$2)); delete $2; }
    ;

%%
