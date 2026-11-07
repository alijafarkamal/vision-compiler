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

// Allocate an integer array and store each provided initializer element.
llvm::Value* ArrayDeclAST::codegen() {
	auto* count = size->codegen();
	if (!count) return nullptr;
	auto* elementType = llvm::Type::getInt64Ty(*cgContext.context);
	auto* array = cgContext.builder->CreateAlloca(elementType, count, name);
	cgContext.namedValues[name] = array;
	for (size_t index = 0; index < elements.size(); ++index) {
		auto* value = elements[index]->codegen();
		if (!value) return nullptr;
		auto* offset = llvm::ConstantInt::get(elementType, index);
		cgContext.builder->CreateStore(value, cgContext.builder->CreateGEP(elementType, array, offset));
	}
	return array;
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

// Store a new value into a scalar slot or the selected array element.
llvm::Value* AssignmentAST::codegen() {
	auto* assigned = value->codegen();
	auto found = cgContext.namedValues.find(name);
	if (!assigned || found == cgContext.namedValues.end()) return nullptr;
	if (index) {
		auto* offset = index->codegen();
		if (!offset) return nullptr;
		auto* type = llvm::Type::getInt64Ty(*cgContext.context);
		cgContext.builder->CreateStore(assigned, cgContext.builder->CreateGEP(type, found->second, offset, "arrayassign"));
	} else cgContext.builder->CreateStore(assigned, found->second);
	return assigned;
}

// Load an array element through a typed LLVM get-element-pointer instruction.
llvm::Value* ArrayAccessAST::codegen() {
	auto found = cgContext.namedValues.find(name);
	auto* offset = index->codegen();
	if (found == cgContext.namedValues.end() || !offset) return nullptr;
	auto* type = llvm::Type::getInt64Ty(*cgContext.context);
	auto* element = cgContext.builder->CreateGEP(type, found->second, offset, "arrayidx");
	return cgContext.builder->CreateLoad(type, element, "arrayload");
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
	// Restore outer bindings after this lexical block has been emitted.
	auto previousValues = cgContext.namedValues;
	llvm::Value* last = nullptr;
	for (auto& statement : statements) {
		if (cgContext.builder->GetInsertBlock()->getTerminator()) break;
		last = statement->codegen();
		if (!last && !cgContext.builder->GetInsertBlock()->getTerminator()) {
			cgContext.namedValues = std::move(previousValues);
			return nullptr;
		}
	}
	cgContext.namedValues = std::move(previousValues);
	// Represent an empty block as a successful no-op for its parent statement.
	return last ? last : llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), 0);
}

// Build native main, emit top-level statements, and terminate the entry block.
llvm::Value* ProgramAST::codegen() {
	// Declare every user function before emitting bodies to support forward calls.
	for (auto& statement : statements) {
		auto* function = dynamic_cast<FunctionAST*>(statement.get());
		if (!function || cgContext.module->getFunction(function->name)) continue;
		std::vector<llvm::Type*> parameters(function->params.size(), llvm::Type::getInt64Ty(*cgContext.context));
		auto* signature = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), parameters, false);
		llvm::Function::Create(signature, llvm::Function::ExternalLinkage, function->name, cgContext.module.get());
	}
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

// Lower conditional statements to LLVM then, else, and merge basic blocks.
llvm::Value* IfAST::codegen() {
	auto* test = condition->codegen();
	if (!test) return nullptr;
	auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), 0);
	auto* predicate = cgContext.builder->CreateICmpNE(test, zero, "ifcond");
	auto* function = cgContext.builder->GetInsertBlock()->getParent();
	auto* thenBlockIR = llvm::BasicBlock::Create(*cgContext.context, "then", function);
	auto* elseBlockIR = llvm::BasicBlock::Create(*cgContext.context, "else");
	auto* mergeBlock = llvm::BasicBlock::Create(*cgContext.context, "ifcont");
	cgContext.builder->CreateCondBr(predicate, thenBlockIR, elseBlock ? elseBlockIR : mergeBlock);
	cgContext.builder->SetInsertPoint(thenBlockIR);
	thenBlock->codegen();
	if (!cgContext.builder->GetInsertBlock()->getTerminator()) cgContext.builder->CreateBr(mergeBlock);
	if (elseBlock) {
		function->insert(function->end(), elseBlockIR);
		cgContext.builder->SetInsertPoint(elseBlockIR);
		elseBlock->codegen();
		if (!cgContext.builder->GetInsertBlock()->getTerminator()) cgContext.builder->CreateBr(mergeBlock);
	}
	function->insert(function->end(), mergeBlock);
	cgContext.builder->SetInsertPoint(mergeBlock);
	return mergeBlock;
}

// Lower repeat-while loops to a condition block, body block, and exit block.
llvm::Value* WhileAST::codegen() {
	auto* function = cgContext.builder->GetInsertBlock()->getParent();
	auto* conditionBlockIR = llvm::BasicBlock::Create(*cgContext.context, "whilecond", function);
	auto* bodyBlockIR = llvm::BasicBlock::Create(*cgContext.context, "whilebody");
	auto* exitBlockIR = llvm::BasicBlock::Create(*cgContext.context, "whileexit");
	cgContext.builder->CreateBr(conditionBlockIR);
	cgContext.builder->SetInsertPoint(conditionBlockIR);
	auto* test = condition->codegen();
	if (!test) return nullptr;
	auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), 0);
	cgContext.builder->CreateCondBr(cgContext.builder->CreateICmpNE(test, zero), bodyBlockIR, exitBlockIR);
	function->insert(function->end(), bodyBlockIR);
	cgContext.builder->SetInsertPoint(bodyBlockIR);
	body->codegen();
	if (!cgContext.builder->GetInsertBlock()->getTerminator()) cgContext.builder->CreateBr(conditionBlockIR);
	function->insert(function->end(), exitBlockIR);
	cgContext.builder->SetInsertPoint(exitBlockIR);
	return exitBlockIR;
}

// Lower exclusive range loops with a scoped 64-bit induction variable.
llvm::Value* RangeLoopAST::codegen() {
	auto* initial = start->codegen();
	auto* limit = end->codegen();
	auto* function = cgContext.builder->GetInsertBlock()->getParent();
	if (!initial || !limit) return nullptr;
	auto previous = cgContext.namedValues.find(var);
	auto* previousSlot = previous == cgContext.namedValues.end() ? nullptr : previous->second;
	auto* slot = createEntryAlloca(function, var, llvm::Type::getInt64Ty(*cgContext.context));
	cgContext.builder->CreateStore(initial, slot);
	cgContext.namedValues[var] = slot;
	auto* conditionBlockIR = llvm::BasicBlock::Create(*cgContext.context, "rangecond", function);
	auto* bodyBlockIR = llvm::BasicBlock::Create(*cgContext.context, "rangebody");
	auto* stepBlockIR = llvm::BasicBlock::Create(*cgContext.context, "rangestep");
	auto* exitBlockIR = llvm::BasicBlock::Create(*cgContext.context, "rangeexit");
	cgContext.builder->CreateBr(conditionBlockIR);
	cgContext.builder->SetInsertPoint(conditionBlockIR);
	auto* current = cgContext.builder->CreateLoad(slot->getAllocatedType(), slot, var);
	cgContext.builder->CreateCondBr(cgContext.builder->CreateICmpSLT(current, limit), bodyBlockIR, exitBlockIR);
	function->insert(function->end(), bodyBlockIR);
	cgContext.builder->SetInsertPoint(bodyBlockIR);
	body->codegen();
	if (!cgContext.builder->GetInsertBlock()->getTerminator()) cgContext.builder->CreateBr(stepBlockIR);
	function->insert(function->end(), stepBlockIR);
	cgContext.builder->SetInsertPoint(stepBlockIR);
	current = cgContext.builder->CreateLoad(slot->getAllocatedType(), slot, var);
	cgContext.builder->CreateStore(cgContext.builder->CreateAdd(current, llvm::ConstantInt::get(current->getType(), 1)), slot);
	cgContext.builder->CreateBr(conditionBlockIR);
	function->insert(function->end(), exitBlockIR);
	cgContext.builder->SetInsertPoint(exitBlockIR);
	if (previousSlot) cgContext.namedValues[var] = previousSlot;
	else cgContext.namedValues.erase(var);
	return exitBlockIR;
}

// Lower functions with 64-bit parameters, scoped locals, and a default return.
llvm::Value* FunctionAST::codegen() {
	std::vector<llvm::Type*> parameterTypes(params.size(), llvm::Type::getInt64Ty(*cgContext.context));
	auto* functionType = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), parameterTypes, false);
	auto* function = cgContext.module->getFunction(name);
	if (!function) function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name, cgContext.module.get());
	auto* previousBlock = cgContext.builder->GetInsertBlock();
	auto previousValues = cgContext.namedValues;
	auto* entry = llvm::BasicBlock::Create(*cgContext.context, "entry", function);
	cgContext.builder->SetInsertPoint(entry);
	size_t index = 0;
	for (auto& argument : function->args()) {
		argument.setName(params[index]);
		auto* slot = createEntryAlloca(function, params[index], llvm::Type::getInt64Ty(*cgContext.context));
		cgContext.builder->CreateStore(&argument, slot);
		cgContext.namedValues[params[index++]] = slot;
	}
	body->codegen();
	if (!cgContext.builder->GetInsertBlock()->getTerminator())
		cgContext.builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), 0));
	cgContext.namedValues = std::move(previousValues);
	if (previousBlock) cgContext.builder->SetInsertPoint(previousBlock);
	else cgContext.builder->ClearInsertionPoint();
	return function;
}

// Declare core image runtime wrappers with signatures matching vision_runtime.cpp.
static llvm::Function* getImageRuntimeFunction(const std::string& name) {
	auto* i64 = llvm::Type::getInt64Ty(*cgContext.context);
	auto* string = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*cgContext.context));
	std::vector<llvm::Type*> parameters;
	std::string symbol;
	if (name == "load_image") { symbol = "edge_load_image"; parameters = {string}; }
	else if (name == "resize") { symbol = "edge_resize"; parameters = {i64, i64, i64}; }
	else if (name == "grayscale" || name == "show") { symbol = name == "show" ? "edge_show" : "edge_grayscale"; parameters = {i64}; }
	else if (name == "blur") { symbol = "edge_blur"; parameters = {i64, i64}; }
	else if (name == "save") { symbol = "edge_save"; parameters = {i64, string}; }
	// Map camera and model operations to their separate runtime wrappers.
	else if (name == "open_camera") { symbol = "edge_open_camera"; parameters = {i64}; }
	else if (name == "camera_read") { symbol = "edge_camera_read"; parameters = {i64}; }
	else if (name == "load_model") { symbol = "edge_load_model"; parameters = {string}; }
	else if (name == "detect") { symbol = "edge_detect"; parameters = {i64, i64}; }
	else return nullptr;
	if (auto* existing = cgContext.module->getFunction(symbol)) return existing;
	auto* type = llvm::FunctionType::get(i64, parameters, false);
	return llvm::Function::Create(type, llvm::Function::ExternalLinkage, symbol, cgContext.module.get());
}

// Lower calls to previously emitted EdgeFlow functions after checking arity.
llvm::Value* CallAST::codegen() {
	auto* function = cgContext.module->getFunction(callee);
	if (!function) function = getImageRuntimeFunction(callee);
	if (!function || function->arg_size() != args.size()) return nullptr;
	std::vector<llvm::Value*> arguments;
	for (auto& argument : args) {
		auto* value = argument->codegen();
		if (!value) return nullptr;
		arguments.push_back(value);
	}
	return cgContext.builder->CreateCall(function, arguments, "calltmp");
}

// Match give values to the return type of the enclosing LLVM function.
llvm::Value* ReturnAST::codegen() {
	auto* result = value ? value->codegen() : llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), 0);
	if (!result) return nullptr;
	auto* returnType = cgContext.builder->GetInsertBlock()->getParent()->getReturnType();
	if (result->getType() != returnType) {
		if (!result->getType()->isIntegerTy() || !returnType->isIntegerTy()) return nullptr;
		result = cgContext.builder->CreateIntCast(result, returnType, true, "retcast");
	}
	return cgContext.builder->CreateRet(result);
}

// Store string literals as null-terminated global constants for runtime calls.
llvm::Value* StringExprAST::codegen() {
	auto text = val.size() >= 2 ? val.substr(1, val.size() - 2) : val;
	return cgContext.builder->CreateGlobalStringPtr(text, "str");
}
