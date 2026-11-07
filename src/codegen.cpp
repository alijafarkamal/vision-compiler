#include "AST.h"
#include "CodeGen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <iostream>

CodeGenContext cgContext;

// Lower an EdgeFlow integer literal to a signed 64-bit LLVM constant.
llvm::Value* NumberExprAST::codegen() {
	return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), val, true);
}

// Place stack allocations in the entry block for stable local storage.
static llvm::AllocaInst* createEntryAlloca(llvm::Function* function, const std::string& name, llvm::Type* type) {
	llvm::IRBuilder<> entryBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
	return entryBuilder.CreateAlloca(type, nullptr, name);
}

// Lower a scalar declaration by allocating and initializing its local slot.
llvm::Value* VarDeclAST::codegen() {
	auto* function = cgContext.builder->GetInsertBlock()->getParent();
	auto* initial = init ? init->codegen() : llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), 0);
	if (!initial) return nullptr;
	auto* slot = createEntryAlloca(function, name, llvm::Type::getInt64Ty(*cgContext.context));
	cgContext.builder->CreateStore(initial, slot);
	cgContext.namedValues[name] = slot;
	return initial;
}

// Read a scalar by loading its previously allocated stack slot.
llvm::Value* VariableExprAST::codegen() {
	auto found = cgContext.namedValues.find(name);
	if (found == cgContext.namedValues.end()) {
		std::cerr << "Unknown variable name " << name << std::endl;
		return nullptr;
	}
	return cgContext.builder->CreateLoad(found->second->getAllocatedType(), found->second, name);
}

// Store a new value into an existing scalar variable slot.
llvm::Value* AssignmentAST::codegen() {
	auto* assigned = value->codegen();
	auto found = cgContext.namedValues.find(name);
	if (!assigned || found == cgContext.namedValues.end() || index) return nullptr;
	cgContext.builder->CreateStore(assigned, found->second);
	return assigned;
}

// Lower integer arithmetic while preserving EdgeFlow's signed 64-bit values.
llvm::Value* BinaryExprAST::codegen() {
	auto* left = lhs->codegen();
	auto* right = rhs->codegen();
	if (!left || !right) return nullptr;
	if (op == "+") return cgContext.builder->CreateAdd(left, right, "addtmp");
	if (op == "-") return cgContext.builder->CreateSub(left, right, "subtmp");
	if (op == "*") return cgContext.builder->CreateMul(left, right, "multmp");
	if (op == "/") return cgContext.builder->CreateSDiv(left, right, "divtmp");
	if (op == "%") return cgContext.builder->CreateSRem(left, right, "modtmp");
	return nullptr;
}
