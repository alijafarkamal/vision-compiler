#ifndef AST_H
#define AST_H

#include <vector>
#include <string>
#include <memory>
#include <iostream>

namespace llvm {
    class Value;
}

extern int yylineno;

class ASTNode {
public:
    int line = yylineno;
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
    virtual llvm::Value* codegen() = 0;
    virtual bool checkSemantics() { return true; }
protected:
    void printIndent(int indent) const {
        for (int i = 0; i < indent; ++i) std::cout << "  ";
    }
};

using ExprPtr = std::unique_ptr<ASTNode>;
using StmtPtr = std::unique_ptr<ASTNode>;

class NumberExprAST : public ASTNode {
public:
    long long val;
    NumberExprAST(long long val) : val(val) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class VariableExprAST : public ASTNode {
public:
    std::string name;
    VariableExprAST(const std::string& name) : name(name) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class StringExprAST : public ASTNode {
public:
    std::string val;
    StringExprAST(const std::string& val) : val(val) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class ArrayAccessAST : public ASTNode {
public:
    std::string name;
    ExprPtr index;
    ArrayAccessAST(const std::string& name, ExprPtr index) : name(name), index(std::move(index)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class BinaryExprAST : public ASTNode {
public:
    std::string op;
    ExprPtr lhs, rhs;
    BinaryExprAST(std::string op, ExprPtr lhs, ExprPtr rhs) : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class UnaryExprAST : public ASTNode {
public:
    std::string op;
    ExprPtr operand;
    UnaryExprAST(std::string op, ExprPtr operand) : op(op), operand(std::move(operand)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class CallAST : public ASTNode {
public:
    std::string callee;
    std::vector<ExprPtr> args;
    CallAST(const std::string& callee, std::vector<ExprPtr> args) : callee(callee), args(std::move(args)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class BlockAST : public ASTNode {
public:
    std::vector<StmtPtr> statements;
    BlockAST(std::vector<StmtPtr> statements) : statements(std::move(statements)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class VarDeclAST : public ASTNode {
public:
    std::string name;
    ExprPtr init;
    VarDeclAST(const std::string& name, ExprPtr init) : name(name), init(std::move(init)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class ArrayDeclAST : public ASTNode {
public:
    std::string name;
    ExprPtr size;
    std::vector<ExprPtr> elements;
    ArrayDeclAST(const std::string& name, ExprPtr size, std::vector<ExprPtr> elements)
        : name(name), size(std::move(size)), elements(std::move(elements)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class AssignmentAST : public ASTNode {
public:
    std::string name;
    ExprPtr index; // Optional
    ExprPtr value;
    AssignmentAST(const std::string& name, ExprPtr index, ExprPtr value)
        : name(name), index(std::move(index)), value(std::move(value)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class IfAST : public ASTNode {
public:
    ExprPtr condition;
    std::unique_ptr<BlockAST> thenBlock;
    std::unique_ptr<BlockAST> elseBlock;
    IfAST(ExprPtr cond, std::unique_ptr<BlockAST> thenB, std::unique_ptr<BlockAST> elseB)
        : condition(std::move(cond)), thenBlock(std::move(thenB)), elseBlock(std::move(elseB)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class WhileAST : public ASTNode {
public:
    ExprPtr condition;
    std::unique_ptr<BlockAST> body;
    WhileAST(ExprPtr cond, std::unique_ptr<BlockAST> body)
        : condition(std::move(cond)), body(std::move(body)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class RangeLoopAST : public ASTNode {
public:
    std::string var;
    ExprPtr start, end;
    std::unique_ptr<BlockAST> body;
    RangeLoopAST(const std::string& var, ExprPtr start, ExprPtr end, std::unique_ptr<BlockAST> body)
        : var(var), start(std::move(start)), end(std::move(end)), body(std::move(body)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class ReturnAST : public ASTNode {
public:
    ExprPtr value;
    ReturnAST(ExprPtr value) : value(std::move(value)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class EmitAST : public ASTNode {
public:
    ExprPtr value;
    EmitAST(ExprPtr value) : value(std::move(value)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class FunctionAST : public ASTNode {
public:
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockAST> body;
    FunctionAST(const std::string& name, std::vector<std::string> params, std::unique_ptr<BlockAST> body)
        : name(name), params(std::move(params)), body(std::move(body)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
};

class ProgramAST : public ASTNode {
public:
    std::vector<StmtPtr> statements;
    ProgramAST(std::vector<StmtPtr> statements) : statements(std::move(statements)) {}
    void print(int indent = 0) const override;
    llvm::Value* codegen() override;
    bool checkSemantics() override;
    void optimize();
};

#endif
