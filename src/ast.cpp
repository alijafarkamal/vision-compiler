// Print expression nodes with indentation so --ast output preserves tree shape.
#include "AST.h"

void NumberExprAST::print(int indent) const { printIndent(indent); std::cout << "NumberExpr(" << val << ")\n"; }
void VariableExprAST::print(int indent) const { printIndent(indent); std::cout << "VariableExpr(" << name << ")\n"; }
void StringExprAST::print(int indent) const { printIndent(indent); std::cout << "StringExpr(" << val << ")\n"; }
void ArrayAccessAST::print(int indent) const { printIndent(indent); std::cout << "ArrayAccess(" << name << ")\n"; index->print(indent + 1); }
void BinaryExprAST::print(int indent) const { printIndent(indent); std::cout << "BinaryExpr(" << op << ")\n"; lhs->print(indent + 1); rhs->print(indent + 1); }
void UnaryExprAST::print(int indent) const { printIndent(indent); std::cout << "UnaryExpr(" << op << ")\n"; operand->print(indent + 1); }
void CallAST::print(int indent) const { printIndent(indent); std::cout << "CallExpr(" << callee << ")\n"; for (const auto& arg : args) arg->print(indent + 1); }
