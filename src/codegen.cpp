#include "AST.h"
#include "CodeGen.h"
#include <llvm/IR/Constants.h>

CodeGenContext cgContext;

// Lower an EdgeFlow integer literal to a signed 64-bit LLVM constant.
llvm::Value* NumberExprAST::codegen() {
	return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*cgContext.context), val, true);
}
