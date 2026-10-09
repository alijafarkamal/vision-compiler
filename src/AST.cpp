#include "AST.h"
#include <iostream>

void NumberExprAST::print(int indent) const { printIndent(indent); std::cout << "NumberExpr(" << val << ")\n"; }
void VariableExprAST::print(int indent) const { printIndent(indent); std::cout << "VariableExpr(" << name << ")\n"; }
void StringExprAST::print(int indent) const { printIndent(indent); std::cout << "StringExpr(" << val << ")\n"; }
void ArrayAccessAST::print(int indent) const { printIndent(indent); std::cout << "ArrayAccess(" << name << ")\n"; index->print(indent + 1); }
void BinaryExprAST::print(int indent) const { printIndent(indent); std::cout << "BinaryExpr(" << op << ")\n"; lhs->print(indent + 1); rhs->print(indent + 1); }
void UnaryExprAST::print(int indent) const { printIndent(indent); std::cout << "UnaryExpr(" << op << ")\n"; operand->print(indent + 1); }
void CallAST::print(int indent) const { printIndent(indent); std::cout << "CallExpr(" << callee << ")\n"; for (const auto& arg : args) arg->print(indent + 1); }
void BlockAST::print(int indent) const { printIndent(indent); std::cout << "Block\n"; for (const auto& stmt : statements) stmt->print(indent + 1); }
void VarDeclAST::print(int indent) const { printIndent(indent); std::cout << "VarDecl(" << name << ")\n"; if (init) init->print(indent + 1); }
void ArrayDeclAST::print(int indent) const { printIndent(indent); std::cout << "ArrayDecl(" << name << ")\n"; if (size) size->print(indent + 1); for (const auto& el : elements) el->print(indent + 1); }
void AssignmentAST::print(int indent) const { printIndent(indent); std::cout << "Assignment(" << name << ")\n"; if (index) { printIndent(indent+1); std::cout << "Index:\n"; index->print(indent + 2); } value->print(indent + 1); }
void IfAST::print(int indent) const { printIndent(indent); std::cout << "If\n"; condition->print(indent + 1); thenBlock->print(indent + 1); if (elseBlock) { printIndent(indent); std::cout << "Else\n"; elseBlock->print(indent + 1); } }
void WhileAST::print(int indent) const { printIndent(indent); std::cout << "While\n"; condition->print(indent + 1); body->print(indent + 1); }
void RangeLoopAST::print(int indent) const { printIndent(indent); std::cout << "RangeLoop(" << var << ")\n"; start->print(indent + 1); end->print(indent + 1); body->print(indent + 1); }
void ReturnAST::print(int indent) const { printIndent(indent); std::cout << "Return\n"; if (value) value->print(indent + 1); }
void EmitAST::print(int indent) const { printIndent(indent); std::cout << "Emit\n"; value->print(indent + 1); }
void FunctionAST::print(int indent) const { printIndent(indent); std::cout << "Function(" << name << ")\n"; for (const auto& p : params) { printIndent(indent + 1); std::cout << "Param(" << p << ")\n"; } body->print(indent + 1); }
void ProgramAST::print(int indent) const { printIndent(indent); std::cout << "Program\n"; for (const auto& stmt : statements) stmt->print(indent + 1); }

void ProgramAST::optimize() {
    for (size_t i = 0; i < statements.size(); ++i) {
        if (!statements[i]) continue;

        // Dead code elimination for If statements
        auto ifAST = dynamic_cast<IfAST*>(statements[i].get());
        if (ifAST) {
            auto numCond = dynamic_cast<NumberExprAST*>(ifAST->condition.get());
            if (numCond) {
                if (numCond->val == 0) {
                    std::cout << "Optimized: Removed dead 'then' branch." << std::endl;
                    if (ifAST->elseBlock) {
                        statements[i] = std::move(ifAST->elseBlock);
                    } else {
                        statements.erase(statements.begin() + i);
                        i--;
                    }
                } else {
                    std::cout << "Optimized: Removed dead 'else' branch." << std::endl;
                    statements[i] = std::move(ifAST->thenBlock);
                }
                continue;
            }
        }

        // Vision optimization: Redundant resize elimination
        auto assign1 = dynamic_cast<AssignmentAST*>(statements[i].get());
        if (assign1 && i + 1 < statements.size()) {
            auto assign2 = dynamic_cast<AssignmentAST*>(statements[i+1].get());
            if (assign2 && assign1->name == assign2->name) {
                auto call1 = dynamic_cast<CallAST*>(assign1->value.get());
                auto call2 = dynamic_cast<CallAST*>(assign2->value.get());
                if (call1 && call2 && call1->callee == "resize" && call2->callee == "resize") {
                    auto var1 = dynamic_cast<VariableExprAST*>(call1->args[0].get());
                    auto var2 = dynamic_cast<VariableExprAST*>(call2->args[0].get());
                    if (var1 && var2 && var1->name == assign1->name && var2->name == assign2->name) {
                        statements.erase(statements.begin() + i);
                        i--;
                        std::cout << "Optimized: Removed redundant image resize." << std::endl;
                    }
                }
            }
        }
    }
}
