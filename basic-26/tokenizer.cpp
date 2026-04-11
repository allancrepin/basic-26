#include "tokenizer.h"

TokenStream tokenize(const std::string& src) {
    TokenStream ts;
    for (size_t i = 0; i < src.size();) {
        char c = src[i];
        if (c == ' ' || c == '\t' || c == '\r') {
            ++i;
            continue;
        }
        if (c == '\n') {
            if (!ts.tokens.empty() && ts.tokens.back().kind != TokKind::Semicolon) {
                ts.tokens.push_back({ TokKind::Semicolon, ";", 0 });
            }
            ++i;
            continue;
        }

        if (c == '"') {
            ++i;
            std::string lit;
            while (i < src.size() && src[i] != '"') {
                if (src[i] == '\\' && i + 1 < src.size()) {
                    ++i;
                    switch (src[i]) {
                    case 'n':  lit += '\n'; break;
                    case 't':  lit += '\t'; break;
                    case '"':  lit += '"'; break;
                    case '\\': lit += '\\'; break;
                    default:   lit += src[i]; break;
                    }
                }
                else {
                    lit += src[i];
                }
                ++i;
            }
            if (i >= src.size()) {
                throw std::runtime_error("unterminated string literal");
            }
            ++i;
            ts.tokens.push_back({ TokKind::StringLiteral, lit, 0 });
            continue;
        }

        if (isdigit((unsigned char)c)) {
            int64_t n = 0;
            std::string lex;
            while (i < src.size() && isdigit((unsigned char)src[i])) {
                n = n * 10 + (src[i] - '0');
                lex += src[i++];
            }
            ts.tokens.push_back({ TokKind::Number, lex, n });
            continue;
        }

        if (isalpha((unsigned char)c)) {
            std::string ident;
            while (i < src.size() && (isalnum((unsigned char)src[i]) || src[i] == '_')) {
                ident += src[i++];
            }
            std::string up(ident.size(), '\0');
            for (size_t j = 0; j < ident.size(); ++j) {
                up[j] = (char)toupper((unsigned char)ident[j]);
            }
            TokKind k = TokKind::Identifier;
            if (up == "IF")     k = TokKind::If;
            if (up == "THEN")   k = TokKind::Then;
            if (up == "LABEL")  k = TokKind::Label;
            if (up == "GOTO")   k = TokKind::Goto;
            if (up == "LET")    k = TokKind::Let;
            if (up == "ARRAY")  k = TokKind::Array;
            if (up == "STRING") k = TokKind::String;
            if (up == "SET")    k = TokKind::Set;
            if (up == "PRINT")  k = TokKind::Print;
            ts.tokens.push_back({ k, ident, 0 });
            continue;
        }

        TokKind k;
        switch (c) {
        case '=':
            if (i + 1 < src.size() && src[i + 1] == '=') {
                ts.tokens.push_back({ TokKind::EqualEqual, "==", 0 });
                i += 2;
                continue;
            }
            k = TokKind::Equal;
            break;
        case '+': k = TokKind::Plus; break;
        case '-': k = TokKind::Minus; break;
        case '*': k = TokKind::Star; break;
        case '/': k = TokKind::Slash; break;
        case '(': k = TokKind::LParen; break;
        case ')': k = TokKind::RParen; break;
        case ';': k = TokKind::Semicolon; break;
        case '>':
            if (i + 1 < src.size() && src[i + 1] == '=') {
                ts.tokens.push_back({ TokKind::GreaterEq, ">=", 0 });
                i += 2;
                continue;
            }
            k = TokKind::Greater;
            break;
        case '<':
            if (i + 1 < src.size() && src[i + 1] == '=') {
                ts.tokens.push_back({ TokKind::LessEq, "<=", 0 });
                i += 2;
                continue;
            }
            k = TokKind::Less;
            break;
        case '!':
            if (i + 1 < src.size() && src[i + 1] == '=') {
                ts.tokens.push_back({ TokKind::NotEqual, "!=", 0 });
                i += 2;
                continue;
            }
            throw std::runtime_error("expected '!='");
        default:
            throw std::runtime_error(std::string("bad char: ") + c);
        }
        ts.tokens.push_back({ k, std::string(1, c), 0 });
        ++i;
    }
    ts.tokens.push_back({ TokKind::Eof, "", 0 });
    return ts;
}

const char* LiteralPool::intern(const std::string& s) {
    auto buf = std::make_unique<char[]>(s.size() + 1);
    memcpy(buf.get(), s.c_str(), s.size() + 1);
    const char* ptr = buf.get();
    pool.push_back(std::move(buf));
    return ptr;
}
