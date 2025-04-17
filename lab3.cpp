#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

int main() {
    llvm::LLVMContext Context;
    auto Module = std::make_unique<llvm::Module>("simple_compiler", Context);
    llvm::IRBuilder<> Builder(Context);

    llvm::FunctionType *FuncType =
        llvm::FunctionType::get(Builder.getInt32Ty(), false);
    llvm::Function *MainFunc =
        llvm::Function::Create(FuncType, llvm::Function::ExternalLinkage, "main", Module.get());

    llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(Context, "entry", MainFunc);
    Builder.SetInsertPoint(EntryBB);

    llvm::AllocaInst *a = Builder.CreateAlloca(Builder.getInt32Ty(), nullptr, "a");
    llvm::AllocaInst *b = Builder.CreateAlloca(Builder.getInt32Ty(), nullptr, "b");
    llvm::AllocaInst *c = Builder.CreateAlloca(Builder.getInt32Ty(), nullptr, "c");
    llvm::AllocaInst *i = Builder.CreateAlloca(Builder.getInt32Ty(), nullptr, "i");

    Builder.CreateStore(llvm::ConstantInt::get(Builder.getInt32Ty(), 10), a);
    Builder.CreateStore(llvm::ConstantInt::get(Builder.getInt32Ty(), 20), b);

    llvm::Value *a_val = Builder.CreateLoad(Builder.getInt32Ty(), a, "a_val");
    llvm::Value *b_val = Builder.CreateLoad(Builder.getInt32Ty(), b, "b_val");
    llvm::Value *sum = Builder.CreateAdd(a_val, b_val, "sum");
    Builder.CreateStore(sum, c);

    llvm::Value *c_val = Builder.CreateLoad(Builder.getInt32Ty(), c, "c_val");
    llvm::Value *cond = Builder.CreateICmpSGT(c_val, llvm::ConstantInt::get(Builder.getInt32Ty(), 25), "cond");

    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(Context, "then", MainFunc);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(Context, "else", MainFunc);
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(Context, "ifcont", MainFunc);
    Builder.CreateCondBr(cond, ThenBB, ElseBB);

    Builder.SetInsertPoint(ThenBB);
    llvm::Value *c_then = Builder.CreateMul(c_val, llvm::ConstantInt::get(Builder.getInt32Ty(), 2), "c_then");
    Builder.CreateStore(c_then, c);
    Builder.CreateBr(MergeBB);

    Builder.SetInsertPoint(ElseBB);
    llvm::Value *c_else = Builder.CreateSub(c_val, llvm::ConstantInt::get(Builder.getInt32Ty(), 5), "c_else");
    Builder.CreateStore(c_else, c);
    Builder.CreateBr(MergeBB);

    Builder.SetInsertPoint(MergeBB);
    llvm::PHINode *c_phi = Builder.CreatePHI(Builder.getInt32Ty(), 2, "c_phi");
    c_phi->addIncoming(c_then, ThenBB);
    c_phi->addIncoming(c_else, ElseBB);
    Builder.CreateStore(c_phi, c);

    Builder.CreateStore(llvm::ConstantInt::get(Builder.getInt32Ty(), 0), i);
    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(Context, "loop", MainFunc);
    llvm::BasicBlock *AfterLoopBB = llvm::BasicBlock::Create(Context, "afterloop", MainFunc);
    Builder.CreateBr(LoopBB);

    Builder.SetInsertPoint(LoopBB);
    llvm::PHINode *i_phi = Builder.CreatePHI(Builder.getInt32Ty(), 2, "i_phi");
    i_phi->addIncoming(llvm::ConstantInt::get(Builder.getInt32Ty(), 0), MergeBB);

    llvm::Value *c_loop = Builder.CreateLoad(Builder.getInt32Ty(), c, "c_loop");
    llvm::Value *i_val = i_phi;
    llvm::Value *new_c = Builder.CreateAdd(c_loop, i_val, "new_c");
    Builder.CreateStore(new_c, c);

    llvm::Value *next_i = Builder.CreateAdd(i_val, llvm::ConstantInt::get(Builder.getInt32Ty(), 1), "next_i");
    llvm::Value *loopCond = Builder.CreateICmpSLT(next_i, llvm::ConstantInt::get(Builder.getInt32Ty(), 3), "loopCond");
    Builder.CreateCondBr(loopCond, LoopBB, AfterLoopBB);
    i_phi->addIncoming(next_i, LoopBB);

    Builder.SetInsertPoint(AfterLoopBB);
    llvm::Value *final_c = Builder.CreateLoad(Builder.getInt32Ty(), c, "final_c");
    Builder.CreateRet(final_c);

    llvm::verifyFunction(*MainFunc);
    Module->print(llvm::outs(), nullptr);

    return 0;
}
