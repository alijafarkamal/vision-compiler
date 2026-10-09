#include "AST.h"
#include "Semantic.h"

SemanticContext semContext;

bool NumberExprAST::checkSemantics() { return true; }

bool VariableExprAST::checkSemantics() {
    int type = semContext.getVariable(name);
    if (type == -1) {
        semContext.reportError(line, "variable `" + name + "` is not defined");
        return false;
    }
    if (type == 1) {
        semContext.reportError(line, "array `" + name + "` misused as scalar");
        return false;
    }
    return true;
}

bool StringExprAST::checkSemantics() { return true; }

bool ArrayAccessAST::checkSemantics() {
    int type = semContext.getVariable(name);
    if (type == -1) {
        semContext.reportError(line, "array `" + name + "` is not defined");
        return false;
    }
    if (type == 0) {
        semContext.reportError(line, "variable `" + name + "` misused as array");
        return false;
    }
    return index->checkSemantics();
}

bool BinaryExprAST::checkSemantics() {
    return lhs->checkSemantics() && rhs->checkSemantics();
}

bool UnaryExprAST::checkSemantics() {
    return operand->checkSemantics();
}

bool CallAST::checkSemantics() {
    if (!semContext.functions.count(callee)) {
        semContext.reportError(line, "function `" + callee + "` is not defined");
        return false;
    }
    int expectedArgs = semContext.functions[callee];
    if (expectedArgs != args.size()) {
        semContext.reportError(line, "wrong number of arguments to function `" + callee + "`. Expected " + std::to_string(expectedArgs) + ", got " + std::to_string(args.size()));
        return false;
    }
    bool ok = true;
    for (auto& arg : args) {
        if (!arg->checkSemantics()) ok = false;
    }
    return ok;
}

bool BlockAST::checkSemantics() {
    bool ok = true;
    for (auto& stmt : statements) {
        if (!stmt->checkSemantics()) ok = false;
    }
    return ok;
}

bool VarDeclAST::checkSemantics() {
    bool ok = true;
    if (init) ok = init->checkSemantics();
    if (!semContext.declareVariable(name, 0)) {
        semContext.reportError(line, "duplicate declaration of variable `" + name + "`");
        ok = false;
    }
    return ok;
}

bool ArrayDeclAST::checkSemantics() {
    bool ok = true;
    if (size) ok = size->checkSemantics();
    for (auto& el : elements) {
        if (!el->checkSemantics()) ok = false;
    }
    if (!semContext.declareVariable(name, 1)) {
        semContext.reportError(line, "duplicate declaration of array `" + name + "`");
        ok = false;
    }
    return ok;
}

bool AssignmentAST::checkSemantics() {
    bool ok = true;
    int type = semContext.getVariable(name);
    if (type == -1) {
        semContext.reportError(line, "variable `" + name + "` is not defined");
        ok = false;
    } else {
        if (index) {
            if (type == 0) {
                semContext.reportError(line, "variable `" + name + "` misused as array");
                ok = false;
            }
            if (!index->checkSemantics()) ok = false;
        } else {
            if (type == 1) {
                semContext.reportError(line, "array `" + name + "` misused as scalar");
                ok = false;
            }
        }
    }
    if (!value->checkSemantics()) ok = false;
    return ok;
}

bool IfAST::checkSemantics() {
    bool ok = condition->checkSemantics();
    semContext.enterScope();
    if (!thenBlock->checkSemantics()) ok = false;
    semContext.exitScope();
    if (elseBlock) {
        semContext.enterScope();
        if (!elseBlock->checkSemantics()) ok = false;
        semContext.exitScope();
    }
    return ok;
}

bool WhileAST::checkSemantics() {
    bool ok = condition->checkSemantics();
    semContext.enterScope();
    if (!body->checkSemantics()) ok = false;
    semContext.exitScope();
    return ok;
}

bool RangeLoopAST::checkSemantics() {
    bool ok = true;
    if (!start->checkSemantics()) ok = false;
    if (!end->checkSemantics()) ok = false;
    
    semContext.enterScope();
    if (!semContext.declareVariable(var, 0)) {
        semContext.reportError(line, "duplicate declaration of loop variable `" + var + "`");
        ok = false;
    }
    if (!body->checkSemantics()) ok = false;
    semContext.exitScope();
    return ok;
}

bool ReturnAST::checkSemantics() {
    if (value) return value->checkSemantics();
    return true;
}

bool EmitAST::checkSemantics() {
    return value->checkSemantics();
}

bool FunctionAST::checkSemantics() {
    semContext.enterScope();
    bool ok = true;
    for (auto& p : params) {
        if (!semContext.declareVariable(p, 0)) {
            semContext.reportError(line, "duplicate parameter `" + p + "`");
            ok = false;
        }
    }
    if (!body->checkSemantics()) ok = false;
    semContext.exitScope();
    return ok;
}

bool ProgramAST::checkSemantics() {
    // First pass for function declarations
    for (auto& stmt : statements) {
        if (auto func = dynamic_cast<FunctionAST*>(stmt.get())) {
            if (semContext.functions.count(func->name)) {
                semContext.reportError(func->line, "duplicate function `" + func->name + "`");
            } else {
                semContext.functions[func->name] = func->params.size();
            }
        }
    }
    bool ok = true;
    for (auto& stmt : statements) {
        if (!stmt->checkSemantics()) ok = false;
    }
    return ok;
}
