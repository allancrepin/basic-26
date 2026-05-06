#include "codegen.h"

namespace {
void rt_string_dim(RuntimeEnv* e, int slot, int64_t len) {
    assert(slot < e->n_strings);
    free(e->strings[slot].data);
    e->strings[slot].data = (int8_t*)calloc(len, 1);
    if (!e->strings[slot].data) {
        throw std::bad_alloc{};
    }
    e->strings[slot].len = len;
}
}

CodeGen::CodeGen(TokenStream& ts_, asmjit::x86::Assembler& as_, VarRegistry& reg_)
    : ts(ts_), as(as_), reg(reg_), pool(ownedPool) {}

CodeGen::CodeGen(TokenStream& ts_, asmjit::x86::Assembler& as_, VarRegistry& reg_, LiteralPool& pool_)
    : ts(ts_), as(as_), reg(reg_), pool(pool_) {}

asmjit::Label& CodeGen::getLabel(const std::string& name) {
    auto [it, ins] = labelMap.emplace(name, asmjit::Label{});
    if (ins) {
        it->second = as.new_label();
    }
    return it->second;
}

asmjit::x86::Mem CodeGen::mem(int slot) {
    return asmjit::x86::ptr(asmjit::x86::rsp, 32 + slot * 8);
}

void CodeGen::load(asmjit::x86::Gp reg, int slot) {
    as.mov(reg, mem(slot));
}

void CodeGen::store(int slot, asmjit::x86::Gp reg) {
    as.mov(mem(slot), reg);
}

int CodeGen::emitImm(int64_t val) {
    int s = slots.alloc();
    as.mov(kScratch1, asmjit::Imm(val));
    store(s, kScratch1);
    return s;
}

void CodeGen::emitCall(void* fnPtr) {
    as.mov(asmjit::x86::r11, asmjit::Imm((uint64_t)fnPtr));
    as.call(asmjit::x86::r11);
}

int CodeGen::emitVarLoad(int varSlot) {
    int tmp = slots.alloc();
    as.mov(kScratch1, asmjit::x86::ptr(kEnv, 0));
    as.mov(kScratch1, asmjit::x86::ptr(kScratch1, varSlot * 8));
    store(tmp, kScratch1);
    return tmp;
}

void CodeGen::emitVarStore(int varSlot, int tmpSlot) {
    as.mov(kScratch2, asmjit::x86::ptr(kEnv, 0));
    load(kScratch1, tmpSlot);
    as.mov(asmjit::x86::ptr(kScratch2, varSlot * 8), kScratch1);
}

int CodeGen::emitBinOp(TokKind op, int lhs, int rhs) {
    if (op == TokKind::Slash) {
        load(kScratch1, lhs);
        load(kScratch2, rhs);
        as.cqo();
        as.idiv(kScratch2);
        store(lhs, kScratch1);
    }
    else {
        load(kScratch1, lhs);
        switch (op) {
        case TokKind::Plus:  as.add(kScratch1, mem(rhs)); break;
        case TokKind::Minus: as.sub(kScratch1, mem(rhs)); break;
        case TokKind::Star:  as.imul(kScratch1, mem(rhs)); break;
        default: break;
        }
        store(lhs, kScratch1);
    }
    slots.free(rhs);
    return lhs;
}

int CodeGen::parseStatement() {
    if (ts.at(TokKind::Let)) return parseDecl();
    if (ts.at(TokKind::Array)) return parseArrayDecl();
    if (ts.at(TokKind::String)) return parseStringDecl();
    if (ts.at(TokKind::Set)) return parseSetStatement();
    if (ts.at(TokKind::Label)) return parseLabelDecl();
    if (ts.at(TokKind::Goto)) return parseGoto();
    if (ts.at(TokKind::If)) return parseIf();
    if (ts.at(TokKind::Print)) return parsePrint();
	if (ts.at(TokKind::Input)) return parseInput();

    if (ts.at(TokKind::Identifier)) {
        size_t saved = ts.pos;
        std::string name = ts.consume().lexeme;
        if (ts.at(TokKind::Equal)) {
            VarInfo info = reg.lookup(name);
            if (info.type == Type::String) {
                ts.consume();
                return parseStringAssign(info);
            }
        }
        ts.pos = saved;
    }

    return parseExpr();
}

int CodeGen::parsePrint() {
    ts.expect(TokKind::Print);

    bool isString = false;
    if (ts.at(TokKind::StringLiteral)) {
        isString = true;
    }
    else if (ts.at(TokKind::Identifier)) {
        VarInfo info = reg.lookup(ts.peek().lexeme);
        isString = (info.type == Type::String);
    }

    if (isString) {
        int tmpSlot = reg.allocStringSlot();

        auto guard = [&]() {
            as.mov(asmjit::x86::rcx, kEnv);
            as.mov(asmjit::x86::rdx, asmjit::Imm(tmpSlot));
            emitCall((void*)rt_string_free);
            reg.freeStringSlot(tmpSlot);
        };

        try {
            emitStringExprAssign(tmpSlot);
        }
        catch (...) {
            reg.freeStringSlot(tmpSlot);
            throw;
        }

        as.mov(asmjit::x86::rcx, kEnv);
        as.mov(asmjit::x86::rdx, asmjit::Imm(tmpSlot));
        emitCall((void*)rt_print_string);

        guard();
    }
    else {
        int valSlot = parseExpr();
        load(kScratch1, valSlot);
        as.mov(asmjit::x86::rcx, kScratch1);
        emitCall((void*)rt_print_int);
        slots.free(valSlot);
    }

    if (ts.at(TokKind::Semicolon)) ts.consume();
    return emitImm(0);
}

int CodeGen::parseInput() {
    ts.expect(TokKind::Input);
    std::string name = ts.expect(TokKind::Identifier).lexeme;
    if (ts.at(TokKind::Semicolon)) ts.consume();
    VarInfo info = reg.lookup(name);
    if (info.type != Type::Int)
        throw std::runtime_error("input can only be used with int variables");
    as.mov(asmjit::x86::rcx, kEnv);
    as.mov(asmjit::x86::rdx, asmjit::Imm(info.slot));
    emitCall((void*)rt_input);
    return emitImm(0);
}

int CodeGen::parseDecl() {
    ts.expect(TokKind::Let);
    std::string name = ts.expect(TokKind::Identifier).lexeme;
    ts.expect(TokKind::Equal);
    int rhsSlot = parseExpr();
    if (ts.at(TokKind::Semicolon)) ts.consume();
    VarInfo info = reg.declare(name, Type::Int);
    emitVarStore(info.slot, rhsSlot);
    slots.free(rhsSlot);
    return emitVarLoad(info.slot);
}

int CodeGen::parseLabelDecl() {
    ts.expect(TokKind::Label);
    std::string name = ts.expect(TokKind::Identifier).lexeme;
    if (ts.at(TokKind::Semicolon)) ts.consume();
    as.bind(getLabel(name));
    return emitImm(0);
}

int CodeGen::parseArrayDecl() {
    ts.expect(TokKind::Array);
    std::string name = ts.expect(TokKind::Identifier).lexeme;
    ts.expect(TokKind::LParen);
    int lenSlot = parseExpr();
    ts.expect(TokKind::RParen);
    if (ts.at(TokKind::Semicolon)) ts.consume();

    VarInfo info = reg.declare(name, Type::Array);

    as.mov(asmjit::x86::rcx, kEnv);
    as.mov(asmjit::x86::rdx, asmjit::Imm(info.slot));
    load(asmjit::x86::r8, lenSlot);
    emitCall((void*)rt_array_dim);
    slots.free(lenSlot);

    return emitImm(0);
}

int CodeGen::parseStringDecl() {
    ts.expect(TokKind::String);
    std::string name = ts.expect(TokKind::Identifier).lexeme;
    VarInfo info = reg.declare(name, Type::String);

    if (ts.at(TokKind::LParen)) {
        ts.consume();
        int lenSlot = parseExpr();
        ts.expect(TokKind::RParen);
        if (ts.at(TokKind::Semicolon)) ts.consume();

        as.mov(asmjit::x86::rcx, kEnv);
        as.mov(asmjit::x86::rdx, asmjit::Imm(info.slot));
        load(asmjit::x86::r8, lenSlot);
        emitCall((void*)rt_string_dim);
        slots.free(lenSlot);
    }
    else {
        ts.expect(TokKind::Equal);
        emitStringExprAssign(info.slot);
        if (ts.at(TokKind::Semicolon)) ts.consume();
    }
    return emitImm(0);
}

int CodeGen::parseStringAssign(VarInfo info) {
    emitStringExprAssign(info.slot);
    if (ts.at(TokKind::Semicolon)) ts.consume();
    return emitImm(0);
}

CodeGen::StrVal CodeGen::emitStringPrimary() {
    if (ts.at(TokKind::StringLiteral)) {
        std::string lit = ts.consume().lexeme;
        const char* ptr = pool.intern(lit);

        int tmpStrSlot = reg.allocStringSlot();

        as.mov(asmjit::x86::rcx, kEnv);
        as.mov(asmjit::x86::rdx, asmjit::Imm(tmpStrSlot));
        as.mov(asmjit::x86::r8, asmjit::Imm((uint64_t)ptr));
        emitCall((void*)rt_string_assign);
        return { tmpStrSlot, true };
    }
    if (ts.at(TokKind::Identifier)) {
        std::string name = ts.consume().lexeme;
        VarInfo info = reg.lookup(name);
        if (info.type != Type::String)
            throw std::runtime_error("'" + name + "' is not a string variable");
        return { info.slot, false };
    }
    throw std::runtime_error("expected string literal or string variable");
}

void CodeGen::emitStringExprAssign(int dstSlot) {
    std::vector<int> temps;

    auto emitFreeTemps = [&]() {
        for (int t : temps) {
            if (t == dstSlot) continue;

            as.mov(asmjit::x86::rcx, kEnv);
            as.mov(asmjit::x86::rdx, asmjit::Imm(t));
            emitCall((void*)rt_string_free);

            reg.freeStringSlot(t);
        }
    };

    StrVal lhs = emitStringPrimary();
    if (lhs.isTmp) temps.push_back(lhs.slot);

    while (ts.at(TokKind::Plus)) {
        ts.consume();
        StrVal rhs = emitStringPrimary();
        if (rhs.isTmp) temps.push_back(rhs.slot);

        int concatSlot = reg.allocStringSlot();
        temps.push_back(concatSlot);

        as.mov(asmjit::x86::rcx, kEnv);
        as.mov(asmjit::x86::rdx, asmjit::Imm(lhs.slot));
        as.mov(asmjit::x86::r8, asmjit::Imm(rhs.slot));
        emitCall((void*)rt_string_concat);

        int ptrSlot = slots.alloc();
        store(ptrSlot, kScratch1);

        as.mov(asmjit::x86::rcx, kEnv);
        as.mov(asmjit::x86::rdx, asmjit::Imm(concatSlot));
        load(asmjit::x86::r8, ptrSlot);
        emitCall((void*)rt_string_assign_ptr);
        slots.free(ptrSlot);

        lhs = { concatSlot, true };
    }

    if (lhs.slot != dstSlot) {
        as.mov(kScratch1, asmjit::x86::ptr(kEnv, 16));
        as.mov(asmjit::x86::r8, asmjit::x86::ptr(kScratch1, lhs.slot * 16));
        as.mov(asmjit::x86::rcx, kEnv);
        as.mov(asmjit::x86::rdx, asmjit::Imm(dstSlot));
        emitCall((void*)rt_string_assign);
    }

    emitFreeTemps();
}

int CodeGen::parseSetStatement() {
    ts.expect(TokKind::Set);
    std::string name = ts.expect(TokKind::Identifier).lexeme;
    VarInfo info = reg.lookup(name);
    if (info.type != Type::Array && info.type != Type::String)
        throw std::runtime_error("SET requires an array or string variable");

    ts.expect(TokKind::LParen);
    int idxSlot = parseExpr();
    ts.expect(TokKind::RParen);
    ts.expect(TokKind::Equal);

    int valSlot;
    if (info.type == Type::String && ts.at(TokKind::StringLiteral)) {
        std::string lit = ts.consume().lexeme;
        if (lit.size() != 1)
            throw std::runtime_error("SET string(idx) = \"x\" requires exactly one character");
        valSlot = emitImm((int64_t)(uint8_t)lit[0]);
    }
    else {
        valSlot = parseExpr();
    }
    if (ts.at(TokKind::Semicolon)) ts.consume();

    as.mov(asmjit::x86::rcx, kEnv);
    as.mov(asmjit::x86::rdx, asmjit::Imm(info.slot));
    load(asmjit::x86::r8, idxSlot);
    load(asmjit::x86::r9, valSlot);
    if (info.type == Type::Array)
        emitCall((void*)rt_array_set);
    else
        emitCall((void*)rt_string_set);
    slots.free(idxSlot);
    slots.free(valSlot);
    return emitImm(0);
}

int CodeGen::parseGoto() {
    ts.expect(TokKind::Goto);
    std::string name = ts.expect(TokKind::Identifier).lexeme;
    if (ts.at(TokKind::Semicolon)) ts.consume();
    as.jmp(getLabel(name));
    return emitImm(0);
}

int CodeGen::parseIf() {
    ts.expect(TokKind::If);
    int lhsSlot = parseExpr();

    TokKind cmp;
    if (ts.at(TokKind::Greater)) cmp = ts.consume().kind;
    else if (ts.at(TokKind::Less)) cmp = ts.consume().kind;
    else if (ts.at(TokKind::GreaterEq)) cmp = ts.consume().kind;
    else if (ts.at(TokKind::LessEq)) cmp = ts.consume().kind;
    else if (ts.at(TokKind::EqualEqual)) cmp = ts.consume().kind;
    else if (ts.at(TokKind::NotEqual)) cmp = ts.consume().kind;
    else throw std::runtime_error("expected comparison operator in IF");

    int rhsSlot = parseExpr();
    ts.expect(TokKind::Then);

    load(kScratch1, lhsSlot);
    as.cmp(kScratch1, mem(rhsSlot));

    slots.free(lhsSlot);
    slots.free(rhsSlot);

    asmjit::Label skip = as.new_label();
    switch (cmp) {
    case TokKind::Greater:    as.jle(skip); break;
    case TokKind::Less:       as.jge(skip); break;
    case TokKind::GreaterEq:  as.jl(skip); break;
    case TokKind::LessEq:     as.jg(skip); break;
    case TokKind::EqualEqual: as.jne(skip); break;
    case TokKind::NotEqual:   as.je(skip); break;
    default: break;
    }

    int bodySlot = parseStatement();
    slots.free(bodySlot);

    as.bind(skip);

    return emitImm(0);
}

int CodeGen::parseExpr() {
    int lhs = parseTerm();
    while (ts.at(TokKind::Plus) || ts.at(TokKind::Minus)) {
        TokKind op = ts.consume().kind;
        lhs = emitBinOp(op, lhs, parseTerm());
    }
    return lhs;
}

int CodeGen::parseTerm() {
    int lhs = parseUnary();
    while (ts.at(TokKind::Star) || ts.at(TokKind::Slash)) {
        TokKind op = ts.consume().kind;
        lhs = emitBinOp(op, lhs, parseUnary());
    }
    return lhs;
}

int CodeGen::parseUnary() {
    if (ts.at(TokKind::Minus)) {
        ts.consume();
        int v = parseUnary();
        load(kScratch1, v);
        as.neg(kScratch1);
        store(v, kScratch1);
        return v;
    }
    return parsePrimary();
}

int CodeGen::parsePrimary() {
    if (ts.at(TokKind::Number))
        return emitImm(ts.consume().value);

    if (ts.at(TokKind::Identifier)) {
        std::string name = ts.consume().lexeme;
        VarInfo info = reg.lookup(name);

        if (info.type == Type::Array) {
            ts.expect(TokKind::LParen);
            int idxSlot = parseExpr();
            ts.expect(TokKind::RParen);
            as.mov(asmjit::x86::rcx, kEnv);
            as.mov(asmjit::x86::rdx, asmjit::Imm(info.slot));
            load(asmjit::x86::r8, idxSlot);
            emitCall((void*)rt_array_get);
            slots.free(idxSlot);
            int tmp = slots.alloc();
            store(tmp, asmjit::x86::rax);
            return tmp;
        }

        if (info.type == Type::String) {
            ts.expect(TokKind::LParen);
            int idxSlot = parseExpr();
            ts.expect(TokKind::RParen);
            as.mov(asmjit::x86::rcx, kEnv);
            as.mov(asmjit::x86::rdx, asmjit::Imm(info.slot));
            load(asmjit::x86::r8, idxSlot);
            emitCall((void*)rt_string_get);
            slots.free(idxSlot);
            int tmp = slots.alloc();
            store(tmp, asmjit::x86::rax);
            return tmp;
        }

        if (info.type == Type::Int)
            return emitVarLoad(info.slot);

        throw std::runtime_error("unexpected variable type in expression");
    }

    if (ts.at(TokKind::LParen)) {
        ts.consume();
        int v = parseExpr();
        ts.expect(TokKind::RParen);
        return v;
    }
    throw std::runtime_error("expected number, identifier, or '('");
}

void CodeGen::emitPrologue() {
    as.push(asmjit::x86::r12);
    as.mov(kEnv, asmjit::x86::rcx);
    as.sub(asmjit::x86::rsp, asmjit::Imm(0));
}

void CodeGen::patchAndEmitEpilogue(int resultSlot) {
    load(asmjit::x86::rax, resultSlot);
    slots.free(resultSlot);

    int frame = slots.frameBytes() + 32;

    as.add(asmjit::x86::rsp, asmjit::Imm(frame));
    as.pop(asmjit::x86::r12);
    as.ret();

    uint8_t* base = reinterpret_cast<uint8_t*>(as.code()->text_section()->data());
    size_t sz = as.code()->text_section()->buffer_size();
    for (size_t i = 0; i + 3 < sz; ++i) {
        if (base[i] == 0x48 && base[i + 1] == 0x83 && base[i + 2] == 0xEC) {
            base[i + 3] = (uint8_t)frame;
            break;
        }
        if (base[i] == 0x48 && base[i + 1] == 0x81 && base[i + 2] == 0xEC) {
            *reinterpret_cast<int32_t*>(base + i + 3) = frame;
            break;
        }
    }
}

void CodeGen::compile() {
    emitPrologue();

    int result = parseStatement();

    while (!ts.at(TokKind::Eof)) {
        if (ts.at(TokKind::Semicolon)) ts.consume();
        if (ts.at(TokKind::Eof)) break;
        slots.free(result);
        result = parseStatement();
    }

    ts.expect(TokKind::Eof);
    patchAndEmitEpilogue(result);
}
