// Print expression nodes with indentation so --ast output preserves tree shape.
#include "AST.h"

void NumberExprAST::print(int indent) const { printIndent(indent); std::cout << "NumberExpr(" << val << ")\n"; }
void VariableExprAST::print(int indent) const { printIndent(indent); std::cout << "VariableExpr(" << name << ")\n"; }
void StringExprAST::print(int indent) const { printIndent(indent); std::cout << "StringExpr(" << val << ")\n"; }
void ArrayAccessAST::print(int indent) const { printIndent(indent); std::cout << "ArrayAccess(" << name << ")\n"; index->print(indent + 1); }
void BinaryExprAST::print(int indent) const { printIndent(indent); std::cout << "BinaryExpr(" << op << ")\n"; lhs->print(indent + 1); rhs->print(indent + 1); }
void UnaryExprAST::print(int indent) const { printIndent(indent); std::cout << "UnaryExpr(" << op << ")\n"; operand->print(indent + 1); }
void CallAST::print(int indent) const { printIndent(indent); std::cout << "CallExpr(" << callee << ")\n"; for (const auto& arg : args) arg->print(indent + 1); }

// Print statement nodes and their children in source-tree order.
void BlockAST::print(int indent) const { printIndent(indent); std::cout << "Block\n"; for (const auto& stmt : statements) stmt->print(indent + 1); }
void VarDeclAST::print(int indent) const { printIndent(indent); std::cout << "VarDecl(" << name << ")\n"; if (init) init->print(indent + 1); }
void ArrayDeclAST::print(int indent) const { printIndent(indent); std::cout << "ArrayDecl(" << name << ")\n"; if (size) size->print(indent + 1); for (const auto& el : elements) el->print(indent + 1); }
void AssignmentAST::print(int indent) const { printIndent(indent); std::cout << "Assignment(" << name << ")\n"; if (index) index->print(indent + 1); value->print(indent + 1); }
void IfAST::print(int indent) const { printIndent(indent); std::cout << "If\n"; condition->print(indent + 1); thenBlock->print(indent + 1); if (elseBlock) elseBlock->print(indent + 1); }
void WhileAST::print(int indent) const { printIndent(indent); std::cout << "While\n"; condition->print(indent + 1); body->print(indent + 1); }
void RangeLoopAST::print(int indent) const { printIndent(indent); std::cout << "RangeLoop(" << var << ")\n"; start->print(indent + 1); end->print(indent + 1); body->print(indent + 1); }
void ReturnAST::print(int indent) const { printIndent(indent); std::cout << "Return\n"; if (value) value->print(indent + 1); }
void EmitAST::print(int indent) const { printIndent(indent); std::cout << "Emit\n"; value->print(indent + 1); }
void FunctionAST::print(int indent) const { printIndent(indent); std::cout << "Function(" << name << ")\n"; body->print(indent + 1); }
void ProgramAST::print(int indent) const { printIndent(indent); std::cout << "Program\n"; for (const auto& stmt : statements) stmt->print(indent + 1); }
