#include "AST.h"
#include "CodeGen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
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
	auto* comparison = static_cast<llvm::Value*>(nullptr);
	if (op == "==") comparison = cgContext.builder->CreateICmpEQ(left, right, "eqtmp");
	if (op == "!=") comparison = cgContext.builder->CreateICmpNE(left, right, "netmp");
	if (op == "<") comparison = cgContext.builder->CreateICmpSLT(left, right, "lttmp");
	if (op == "<=") comparison = cgContext.builder->CreateICmpSLE(left, right, "letmp");
	if (op == ">") comparison = cgContext.builder->CreateICmpSGT(left, right, "gttmp");
	if (op == ">=") comparison = cgContext.builder->CreateICmpSGE(left, right, "getmp");
	if (comparison) return cgContext.builder->CreateZExt(comparison, llvm::Type::getInt64Ty(*cgContext.context));
	if (op == "and" || op == "or") {
		auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), 0);
		auto* lhsBool = cgContext.builder->CreateICmpNE(left, zero);
		auto* rhsBool = cgContext.builder->CreateICmpNE(right, zero);
		auto* result = op == "and" ? cgContext.builder->CreateAnd(lhsBool, rhsBool) : cgContext.builder->CreateOr(lhsBool, rhsBool);
		return cgContext.builder->CreateZExt(result, llvm::Type::getInt64Ty(*cgContext.context));
	}
	return nullptr;
}

// Lower unary negation and logical not to signed integer LLVM operations.
llvm::Value* UnaryExprAST::codegen() {
	auto* value = operand->codegen();
	if (!value) return nullptr;
	if (op == "-") return cgContext.builder->CreateNeg(value, "negtmp");
	if (op == "not") {
		auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), 0);
		auto* isZero = cgContext.builder->CreateICmpEQ(value, zero);
		return cgContext.builder->CreateZExt(isZero, llvm::Type::getInt64Ty(*cgContext.context));
	}
	return nullptr;
}

// Emit block statements in source order and return the final generated value.
llvm::Value* BlockAST::codegen() {
	llvm::Value* last = nullptr;
	for (auto& statement : statements) last = statement->codegen();
	return last;
}

// Build native main, emit top-level statements, and terminate the entry block.
llvm::Value* ProgramAST::codegen() {
	for (auto& statement : statements)
		if (dynamic_cast<FunctionAST*>(statement.get()) && !statement->codegen()) return nullptr;
	auto* type = llvm::FunctionType::get(llvm::Type::getInt32Ty(*cgContext.context), false);
	auto* function = llvm::Function::Create(type, llvm::Function::ExternalLinkage, "main", cgContext.module.get());
	auto* entry = llvm::BasicBlock::Create(*cgContext.context, "entry", function);
	cgContext.builder->SetInsertPoint(entry);
	for (auto& statement : statements)
		if (!dynamic_cast<FunctionAST*>(statement.get()) && !statement->codegen()) return nullptr;
	if (!entry->getTerminator()) cgContext.builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*cgContext.context), 0));
	return function;
}

// Route EdgeFlow emit values through the native variadic printf function.
llvm::Value* EmitAST::codegen() {
	auto* emitted = value->codegen();
	if (!emitted) return nullptr;
	auto* format = cgContext.builder->CreateGlobalStringPtr(emitted->getType()->isPointerTy() ? "%s\n" : "%lld\n");
	auto* printf = cgContext.module->getFunction("printf");
	if (!printf) {
		auto* type = llvm::FunctionType::get(llvm::Type::getInt32Ty(*cgContext.context), llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*cgContext.context)), true);
		printf = llvm::Function::Create(type, llvm::Function::ExternalLinkage, "printf", cgContext.module.get());
	}
	return cgContext.builder->CreateCall(printf, {format, emitted}, "emitcall");
}
