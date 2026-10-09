#ifndef CODEGEN_H
#define CODEGEN_H

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <memory>
#include <map>
#include <string>

struct CodeGenContext {
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::Module> module;
    std::map<std::string, llvm::AllocaInst*> namedValues;

    CodeGenContext() {
        context = std::make_unique<llvm::LLVMContext>();
        module = std::make_unique<llvm::Module>("EdgeFlowModule", *context);
        builder = std::make_unique<llvm::IRBuilder<>>(*context);
    }
};

extern CodeGenContext cgContext;

#endif
