#pragma once

#include "runtime.h"
#include "tokenizer.h"

struct CodeGen {
    TokenStream& ts;
    asmjit::x86::Assembler& as;
    VarRegistry& reg;
    LiteralPool& pool;
    SlotAllocator slots;

    std::unordered_map<std::string, asmjit::Label> labelMap;

    const asmjit::x86::Gp kEnv = asmjit::x86::r12;
    const asmjit::x86::Gp kScratch1 = asmjit::x86::rax;
    const asmjit::x86::Gp kScratch2 = asmjit::x86::rcx;
    const asmjit::x86::Gp kScratch3 = asmjit::x86::rdx;

    LiteralPool ownedPool;

    struct StrVal {
        int slot;
        bool isTmp;
    };

    CodeGen(TokenStream& ts_, asmjit::x86::Assembler& as_, VarRegistry& reg_);
    CodeGen(TokenStream& ts_, asmjit::x86::Assembler& as_, VarRegistry& reg_, LiteralPool& pool_);

    asmjit::Label& getLabel(const std::string& name);
    asmjit::x86::Mem mem(int slot);
    void load(asmjit::x86::Gp reg, int slot);
    void store(int slot, asmjit::x86::Gp reg);
    int emitImm(int64_t val);
    void emitCall(void* fnPtr);
    int emitVarLoad(int varSlot);
    void emitVarStore(int varSlot, int tmpSlot);
    int emitBinOp(TokKind op, int lhs, int rhs);
    int parseStatement();
	int parseInput();
    int parsePrint();
    int parseDecl();
    int parseLabelDecl();
    int parseArrayDecl();
    int parseStringDecl();
    int parseStringAssign(VarInfo info);
    StrVal emitStringPrimary();
    void emitStringExprAssign(int dstSlot);
    int parseSetStatement();
    int parseGoto();
    int parseIf();
    int parseExpr();
    int parseTerm();
    int parseUnary();
    int parsePrimary();
    void emitPrologue();
    void patchAndEmitEpilogue(int resultSlot);
    void compile();
};
