#pragma once

#include "common.h"

enum class TokKind {
    Let, Set, Array, String, Label, Goto, If, Then, Print, Input,
    Identifier, Equal, EqualEqual, NotEqual,
    Greater, Less, GreaterEq, LessEq,
    Number, StringLiteral,
    Plus, Minus, Star, Slash,
    LParen, RParen, Semicolon, Eof
};

struct Token {
    TokKind kind;
    std::string lexeme;
    int64_t value;
};

struct TokenStream {
    std::vector<Token> tokens;
    size_t pos = 0;

    Token peek() const { return tokens[pos]; }
    Token consume() { return tokens[pos++]; }
    bool at(TokKind k) const { return peek().kind == k; }
    Token expect(TokKind k) {
        if (!at(k)) {
            throw std::runtime_error("unexpected token");
        }
        return consume();
    }
};

TokenStream tokenize(const std::string& src);

struct LiteralPool {
    std::vector<std::unique_ptr<char[]>> pool;
    const char* intern(const std::string& s);
};

struct SlotAllocator {
    int nextSlot = 0;
    std::vector<int> freeList;

    int alloc() {
        if (!freeList.empty()) {
            int s = freeList.back();
            freeList.pop_back();
            return s;
        }
        return nextSlot++;
    }

    void free(int slot) {
        freeList.push_back(slot);
    }

    int frameBytes() const {
        int raw = nextSlot * 8;
        return (raw + 15) & ~15;
    }
};
