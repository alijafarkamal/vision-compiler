#include "AST.h"
#include "CodeGen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

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
