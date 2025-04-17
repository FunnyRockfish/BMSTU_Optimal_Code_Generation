#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

int main() {
    llvm::LLVMContext Context;
    auto Module = std::make_unique<llvm::Module>("my_module", Context);

    llvm::IRBuilder<> Builder(Context);

    llvm::FunctionType *MainFuncType =
        llvm::FunctionType::get(Builder.getInt32Ty(), false);
    llvm::Function *MainFunc =
        llvm::Function::Create(MainFuncType, llvm::Function::ExternalLinkage, "main", Module.get());

    llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(Context, "entry", MainFunc);
    Builder.SetInsertPoint(EntryBB);

    llvm::Value *LHS = llvm::ConstantInt::get(Builder.getInt32Ty(), 353);
    llvm::Value *RHS = llvm::ConstantInt::get(Builder.getInt32Ty(), 48);
    llvm::Value *Sum = Builder.CreateAdd(LHS, RHS, "sum");

    Builder.CreateRet(Sum);

    llvm::verifyFunction(*MainFunc);
    Module->print(llvm::outs(), nullptr);

    return 0;
}
