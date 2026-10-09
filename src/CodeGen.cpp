#include "AST.h"
#include "CodeGen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <iostream>

CodeGenContext cgContext;

static llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function *TheFunction, const std::string &VarName, llvm::Type* type) {
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(type, nullptr, VarName);
}

llvm::Value* NumberExprAST::codegen() {
    return llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, val, true));
}

llvm::Value* VariableExprAST::codegen() {
    llvm::AllocaInst* A = cgContext.namedValues[name];
    if (!A) {
        std::cerr << "Unknown variable name " << name << std::endl;
        return nullptr;
    }
    return cgContext.builder->CreateLoad(A->getAllocatedType(), A, name.c_str());
}

llvm::Value* VarDeclAST::codegen() {
    llvm::Function* TheFunction = cgContext.builder->GetInsertBlock()->getParent();
    llvm::Value* InitVal;
    if (init) {
        InitVal = init->codegen();
        if (!InitVal) return nullptr;
    } else {
        InitVal = llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true));
    }

    llvm::AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, name, llvm::Type::getInt64Ty(*cgContext.context));
    cgContext.builder->CreateStore(InitVal, Alloca);
    cgContext.namedValues[name] = Alloca;
    return InitVal;
}

llvm::Value* AssignmentAST::codegen() {
    llvm::Value* Val = value->codegen();
    if (!Val) return nullptr;

    llvm::AllocaInst* Variable = cgContext.namedValues[name];
    if (!Variable) {
        std::cerr << "Unknown variable name " << name << std::endl;
        return nullptr;
    }

    if (index) {
        llvm::Value* IdxVal = index->codegen();
        if (!IdxVal) return nullptr;
        llvm::Value* GEP = cgContext.builder->CreateGEP(llvm::Type::getInt64Ty(*cgContext.context), Variable, IdxVal, "arrayassign");
        cgContext.builder->CreateStore(Val, GEP);
    } else {
        cgContext.builder->CreateStore(Val, Variable);
    }

    return Val;
}

llvm::Value* BinaryExprAST::codegen() {
    llvm::Value* L = lhs->codegen();
    llvm::Value* R = rhs->codegen();
    if (!L || !R) return nullptr;

    if (op == "+") return cgContext.builder->CreateAdd(L, R, "addtmp");
    if (op == "-") return cgContext.builder->CreateSub(L, R, "subtmp");
    if (op == "*") return cgContext.builder->CreateMul(L, R, "multmp");
    if (op == "/") return cgContext.builder->CreateSDiv(L, R, "divtmp");
    if (op == "%") return cgContext.builder->CreateSRem(L, R, "modtmp");
    if (op == "and") {
        llvm::Value* LBool = cgContext.builder->CreateICmpNE(L, llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true)));
        llvm::Value* RBool = cgContext.builder->CreateICmpNE(R, llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true)));
        llvm::Value* And = cgContext.builder->CreateAnd(LBool, RBool);
        return cgContext.builder->CreateZExt(And, llvm::Type::getInt64Ty(*cgContext.context));
    }
    if (op == "or") {
        llvm::Value* LBool = cgContext.builder->CreateICmpNE(L, llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true)));
        llvm::Value* RBool = cgContext.builder->CreateICmpNE(R, llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true)));
        llvm::Value* Or = cgContext.builder->CreateOr(LBool, RBool);
        return cgContext.builder->CreateZExt(Or, llvm::Type::getInt64Ty(*cgContext.context));
    }

    llvm::Value* cmp = nullptr;
    if (op == "<") cmp = cgContext.builder->CreateICmpSLT(L, R, "cmptmp");
    if (op == "<=") cmp = cgContext.builder->CreateICmpSLE(L, R, "cmptmp");
    if (op == ">") cmp = cgContext.builder->CreateICmpSGT(L, R, "cmptmp");
    if (op == ">=") cmp = cgContext.builder->CreateICmpSGE(L, R, "cmptmp");
    if (op == "==") cmp = cgContext.builder->CreateICmpEQ(L, R, "cmptmp");
    if (op == "!=") cmp = cgContext.builder->CreateICmpNE(L, R, "cmptmp");
    
    if (cmp) {
        return cgContext.builder->CreateIntCast(cmp, llvm::Type::getInt64Ty(*cgContext.context), false, "casttmp");
    }

    return nullptr;
}

llvm::Value* BlockAST::codegen() {
    llvm::Value* last = nullptr;
    for (auto& stmt : statements) {
        last = stmt->codegen();
    }
    return last;
}

llvm::Value* EmitAST::codegen() {
    llvm::Value* V = value->codegen();
    if (!V) return nullptr;

    llvm::Function* printfFunc = cgContext.module->getFunction("printf");
    if (!printfFunc) {
        llvm::FunctionType* printfType = llvm::FunctionType::get(
            llvm::IntegerType::getInt32Ty(*cgContext.context),
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*cgContext.context)),
            true);
        printfFunc = llvm::Function::Create(printfType, llvm::Function::ExternalLinkage, "printf", cgContext.module.get());
    }

    llvm::Value* formatStr = cgContext.builder->CreateGlobalStringPtr("%lld\n");
    return cgContext.builder->CreateCall(printfFunc, {formatStr, V}, "printfCall");
}

llvm::Value* ProgramAST::codegen() {
    // First pass: generate functions
    for (auto& stmt : statements) {
        if (dynamic_cast<FunctionAST*>(stmt.get())) {
            stmt->codegen();
        }
    }

    llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(*cgContext.context), false);
    llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "main", cgContext.module.get());

    llvm::BasicBlock* BB = llvm::BasicBlock::Create(*cgContext.context, "entry", F);
    cgContext.builder->SetInsertPoint(BB);

    for (auto& stmt : statements) {
        if (!dynamic_cast<FunctionAST*>(stmt.get())) {
            stmt->codegen();
        }
    }

    llvm::BasicBlock* currentBB = cgContext.builder->GetInsertBlock();
    if (!currentBB->getTerminator()) {
        cgContext.builder->CreateRet(llvm::ConstantInt::get(*cgContext.context, llvm::APInt(32, 0, true)));
    }
    return F;
}

// Unimplemented
llvm::Value* StringExprAST::codegen() {
    std::string unquoted = val.substr(1, val.length() - 2);
    return cgContext.builder->CreateGlobalStringPtr(unquoted);
}
llvm::Value* ArrayAccessAST::codegen() {
    llvm::AllocaInst* Alloca = cgContext.namedValues[name];
    if (!Alloca) {
        std::cerr << "Unknown array " << name << std::endl;
        return nullptr;
    }
    
    llvm::Value* IdxVal = index->codegen();
    if (!IdxVal) return nullptr;

    llvm::Value* GEP = cgContext.builder->CreateGEP(llvm::Type::getInt64Ty(*cgContext.context), Alloca, IdxVal, "arrayaccess");
    return cgContext.builder->CreateLoad(llvm::Type::getInt64Ty(*cgContext.context), GEP, "arrayload");
}
llvm::Value* UnaryExprAST::codegen() { return nullptr; }
llvm::Value* CallAST::codegen() {
    llvm::Function* CalleeF = cgContext.module->getFunction(callee);
    if (!CalleeF) {
        if (callee == "load_image") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*cgContext.context))}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_load_image", cgContext.module.get());
        } else if (callee == "resize") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::Type::getInt64Ty(*cgContext.context), llvm::Type::getInt64Ty(*cgContext.context), llvm::Type::getInt64Ty(*cgContext.context)}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_resize", cgContext.module.get());
        } else if (callee == "grayscale") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::Type::getInt64Ty(*cgContext.context)}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_grayscale", cgContext.module.get());
        } else if (callee == "blur") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::Type::getInt64Ty(*cgContext.context), llvm::Type::getInt64Ty(*cgContext.context)}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_blur", cgContext.module.get());
        } else if (callee == "show") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::Type::getInt64Ty(*cgContext.context)}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_show", cgContext.module.get());
        } else if (callee == "save") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::Type::getInt64Ty(*cgContext.context), llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*cgContext.context))}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_save", cgContext.module.get());
        } else if (callee == "open_camera") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::Type::getInt64Ty(*cgContext.context)}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_open_camera", cgContext.module.get());
        } else if (callee == "camera_read") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::Type::getInt64Ty(*cgContext.context)}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_camera_read", cgContext.module.get());
        } else if (callee == "load_model") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*cgContext.context))}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_load_model", cgContext.module.get());
        } else if (callee == "detect") {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), {llvm::Type::getInt64Ty(*cgContext.context), llvm::Type::getInt64Ty(*cgContext.context)}, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "edge_detect", cgContext.module.get());
        } else {
            std::cerr << "Unknown function referenced: " << callee << std::endl;
            return nullptr;
        }
    }

    if (CalleeF->arg_size() != args.size()) {
        std::cerr << "Incorrect number of arguments passed to " << callee << std::endl;
        return nullptr;
    }

    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = args.size(); i != e; ++i) {
        ArgsV.push_back(args[i]->codegen());
        if (!ArgsV.back()) return nullptr;
    }

    return cgContext.builder->CreateCall(CalleeF, ArgsV, "calltmp");
}
llvm::Value* ArrayDeclAST::codegen() {
    llvm::Function* TheFunction = cgContext.builder->GetInsertBlock()->getParent();
    
    llvm::Value* SizeVal = size->codegen();
    if (!SizeVal) return nullptr;

    llvm::AllocaInst* Alloca = cgContext.builder->CreateAlloca(llvm::Type::getInt64Ty(*cgContext.context), SizeVal, name);
    cgContext.namedValues[name] = Alloca;

    for (size_t i = 0; i < elements.size(); ++i) {
        llvm::Value* ElVal = elements[i]->codegen();
        if (!ElVal) return nullptr;
        
        llvm::Value* Idx = llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, i, true));
        llvm::Value* GEP = cgContext.builder->CreateGEP(llvm::Type::getInt64Ty(*cgContext.context), Alloca, Idx, "arrayinit");
        cgContext.builder->CreateStore(ElVal, GEP);
    }

    return Alloca;
}
llvm::Value* IfAST::codegen() {
    llvm::Value* CondV = condition->codegen();
    if (!CondV) return nullptr;

    CondV = cgContext.builder->CreateICmpNE(CondV, llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true)), "ifcond");

    llvm::Function* TheFunction = cgContext.builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(*cgContext.context, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(*cgContext.context, "else");
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(*cgContext.context, "ifcont");

    bool hasElse = elseBlock != nullptr;
    cgContext.builder->CreateCondBr(CondV, ThenBB, hasElse ? ElseBB : MergeBB);

    cgContext.builder->SetInsertPoint(ThenBB);
    llvm::Value* ThenV = thenBlock->codegen();
    if (!ThenV) return nullptr;
    cgContext.builder->CreateBr(MergeBB);
    ThenBB = cgContext.builder->GetInsertBlock();

    if (hasElse) {
        TheFunction->insert(TheFunction->end(), ElseBB);
        cgContext.builder->SetInsertPoint(ElseBB);
        llvm::Value* ElseV = elseBlock->codegen();
        if (!ElseV) return nullptr;
        cgContext.builder->CreateBr(MergeBB);
        ElseBB = cgContext.builder->GetInsertBlock();
    }

    TheFunction->insert(TheFunction->end(), MergeBB);
    cgContext.builder->SetInsertPoint(MergeBB);

    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*cgContext.context));
}

llvm::Value* WhileAST::codegen() {
    llvm::Function* TheFunction = cgContext.builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(*cgContext.context, "whilecond", TheFunction);
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(*cgContext.context, "whileloop");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(*cgContext.context, "whileafter");

    cgContext.builder->CreateBr(CondBB);
    cgContext.builder->SetInsertPoint(CondBB);

    llvm::Value* CondV = condition->codegen();
    if (!CondV) return nullptr;
    CondV = cgContext.builder->CreateICmpNE(CondV, llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true)), "cond");
    cgContext.builder->CreateCondBr(CondV, LoopBB, AfterBB);

    TheFunction->insert(TheFunction->end(), LoopBB);
    cgContext.builder->SetInsertPoint(LoopBB);
    llvm::Value* LoopV = body->codegen();
    if (!LoopV) return nullptr;
    cgContext.builder->CreateBr(CondBB);

    TheFunction->insert(TheFunction->end(), AfterBB);
    cgContext.builder->SetInsertPoint(AfterBB);

    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*cgContext.context));
}

llvm::Value* RangeLoopAST::codegen() {
    llvm::Function* TheFunction = cgContext.builder->GetInsertBlock()->getParent();

    llvm::Value* StartVal = start->codegen();
    if (!StartVal) return nullptr;
    
    llvm::Value* EndVal = end->codegen();
    if (!EndVal) return nullptr;

    llvm::AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, var, llvm::Type::getInt64Ty(*cgContext.context));
    cgContext.builder->CreateStore(StartVal, Alloca);

    llvm::AllocaInst* OldVal = cgContext.namedValues[var];
    cgContext.namedValues[var] = Alloca;

    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(*cgContext.context, "forcond", TheFunction);
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(*cgContext.context, "forloop");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(*cgContext.context, "forafter");

    cgContext.builder->CreateBr(CondBB);
    cgContext.builder->SetInsertPoint(CondBB);

    llvm::Value* CurVar = cgContext.builder->CreateLoad(Alloca->getAllocatedType(), Alloca, var.c_str());
    llvm::Value* CondV = cgContext.builder->CreateICmpSLT(CurVar, EndVal, "loopcond");
    cgContext.builder->CreateCondBr(CondV, LoopBB, AfterBB);

    TheFunction->insert(TheFunction->end(), LoopBB);
    cgContext.builder->SetInsertPoint(LoopBB);

    llvm::Value* BodyV = body->codegen();
    if (!BodyV) return nullptr;

    llvm::Value* NextVar = cgContext.builder->CreateAdd(CurVar, llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 1, true)), "nextvar");
    cgContext.builder->CreateStore(NextVar, Alloca);
    cgContext.builder->CreateBr(CondBB);

    TheFunction->insert(TheFunction->end(), AfterBB);
    cgContext.builder->SetInsertPoint(AfterBB);

    if (OldVal) {
        cgContext.namedValues[var] = OldVal;
    } else {
        cgContext.namedValues.erase(var);
    }

    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*cgContext.context));
}
llvm::Value* ReturnAST::codegen() {
    llvm::Value* RetVal = nullptr;
    if (value) {
        RetVal = value->codegen();
    } else {
        RetVal = llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true));
    }
    return cgContext.builder->CreateRet(RetVal);
}
llvm::Value* FunctionAST::codegen() {
    std::vector<llvm::Type*> argTypes(params.size(), llvm::Type::getInt64Ty(*cgContext.context));
    llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(*cgContext.context), argTypes, false);
    llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, name, cgContext.module.get());

    unsigned idx = 0;
    for (auto& arg : F->args()) {
        arg.setName(params[idx++]);
    }

    llvm::BasicBlock* oldBB = cgContext.builder->GetInsertBlock();

    llvm::BasicBlock* BB = llvm::BasicBlock::Create(*cgContext.context, "entry", F);
    cgContext.builder->SetInsertPoint(BB);

    std::map<std::string, llvm::AllocaInst*> oldNamedValues = cgContext.namedValues;
    cgContext.namedValues.clear();

    for (auto& arg : F->args()) {
        llvm::AllocaInst* Alloca = CreateEntryBlockAlloca(F, std::string(arg.getName()), llvm::Type::getInt64Ty(*cgContext.context));
        cgContext.builder->CreateStore(&arg, Alloca);
        cgContext.namedValues[std::string(arg.getName())] = Alloca;
    }

    body->codegen();

    llvm::BasicBlock* currentBB = cgContext.builder->GetInsertBlock();
    if (!currentBB->getTerminator()) {
        cgContext.builder->CreateRet(llvm::ConstantInt::get(*cgContext.context, llvm::APInt(64, 0, true)));
    }

    cgContext.namedValues = oldNamedValues;
    if (oldBB) {
        cgContext.builder->SetInsertPoint(oldBB);
    }
    
    return F;
}
