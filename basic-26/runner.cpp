#include "runner.h"

using JitFn = int64_t(*)(RuntimeEnv*);

static const char* token_kind_name(TokKind kind) {
    switch (kind) {
    case TokKind::Let: return "Let";
    case TokKind::Set: return "Set";
    case TokKind::Array: return "Array";
    case TokKind::String: return "String";
    case TokKind::Label: return "Label";
    case TokKind::Goto: return "Goto";
    case TokKind::If: return "If";
    case TokKind::Then: return "Then";
    case TokKind::Print: return "Print";
    case TokKind::Input: return "Input";
    case TokKind::Identifier: return "Identifier";
    case TokKind::Equal: return "Equal";
    case TokKind::EqualEqual: return "EqualEqual";
    case TokKind::NotEqual: return "NotEqual";
    case TokKind::Greater: return "Greater";
    case TokKind::Less: return "Less";
    case TokKind::GreaterEq: return "GreaterEq";
    case TokKind::LessEq: return "LessEq";
    case TokKind::Number: return "Number";
    case TokKind::StringLiteral: return "StringLiteral";
    case TokKind::Plus: return "Plus";
    case TokKind::Minus: return "Minus";
    case TokKind::Star: return "Star";
    case TokKind::Slash: return "Slash";
    case TokKind::LParen: return "LParen";
    case TokKind::RParen: return "RParen";
    case TokKind::Semicolon: return "Semicolon";
    case TokKind::Eof: return "Eof";
    }
    return "Unknown";
}

static void dump_source(const std::string& source) {
    std::cout << "\n== Source ==\n";
    std::cout << source;
    if (!source.empty() && source.back() != '\n')
        std::cout << "\n";
}

static void dump_tokens(const TokenStream& ts) {
    std::cout << "\n== Tokens ==\n";
    for (size_t i = 0; i < ts.tokens.size(); ++i) {
        const Token& t = ts.tokens[i];
        std::cout << std::setw(4) << i << "  " << std::left << std::setw(14)
            << token_kind_name(t.kind) << std::right;

        if (!t.lexeme.empty())
            std::cout << "  \"" << t.lexeme << "\"";
        if (t.kind == TokKind::Number)
            std::cout << "  value=" << t.value;
        std::cout << "\n";
    }
}

static void dump_codegen(const asmjit::StringLogger& logger, const asmjit::CodeHolder& code) {
    std::cout << "\n== Codegen: AsmJit Log ==\n";
    if (logger.data_size() != 0)
        std::cout << logger.data();
    else
        std::cout << "(no AsmJit log available)\n";

    std::cout << "\n== Codegen: .text Hex ==\n";
    const asmjit::Section* text = code.text_section();
    const uint8_t* data = text ? text->data() : nullptr;
    size_t size = text ? text->buffer_size() : 0;
    if (!data || size == 0) {
        std::cout << "(no machine code emitted)\n";
        return;
    }

    std::ios old_state(nullptr);
    old_state.copyfmt(std::cout);
    for (size_t i = 0; i < size; ++i) {
        if (i % 16 == 0)
            std::cout << std::setw(4) << std::setfill('0') << std::hex << i << ": ";
        std::cout << std::setw(2) << std::setfill('0') << std::hex
            << static_cast<unsigned>(data[i]) << ' ';
        if (i % 16 == 15 || i + 1 == size)
            std::cout << "\n";
    }
    std::cout.copyfmt(old_state);
}

void run(const std::vector<std::string>& lines, int64_t expected) {
    std::string program;
    for (const auto& line : lines)
        program += line + " ";
    if (!program.empty()) program.pop_back();

    std::cout << "Program    : " << program << "\n";

    VarRegistry reg;

    asmjit::JitRuntime rt;
    asmjit::CodeHolder code;
    code.init(rt.environment(), rt.cpu_features());
    asmjit::x86::Assembler as(&code);

    auto ts = tokenize(program);
    CodeGen cg(ts, as, reg);
    cg.compile();

    RuntimeEnv env = RuntimeEnv::alloc(reg);

    JitFn fn;
    asmjit::Error err = rt.add(&fn, &code);
    if (err != asmjit::kErrorOk) {
        std::cerr << "JIT error: " << asmjit::DebugUtils::error_as_string(err) << "\n";
        env.free_all();
        return;
    }

    int64_t result = fn(&env);
    std::cout << "JIT result : " << result
        << (result == expected
            ? "  (CORRECT)"
            : "  (INCORRECT)  expected " + std::to_string(expected))
        << "\n\n";

    rt.release(fn);
    env.free_all();
}

void run_file(const std::string& path, bool debug) {

    bool has_ls_ext = path.size() >= 3 && path.substr(path.size() - 3) == ".ls";
    bool has_b26_ext = path.size() >= 4 && path.substr(path.size() - 4) == ".b26";
    if (!has_ls_ext && !has_b26_ext) {
        std::cerr << "Error: file must have a .ls or .b26 extension (got \"" << path << "\")\n";
        return;
    }

    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: could not open file \"" << path << "\"\n";
        return;
    }

    std::vector<std::string> lines;
    std::ostringstream source_builder;
    std::string line;
    while (std::getline(f, line)) {
        source_builder << line << "\n";
        if (!line.empty())
            lines.push_back(line);
    }

    if (lines.empty()) {
        std::cerr << "Error: file \"" << path << "\" is empty\n";
        return;
    }

    std::string program;
    for (const auto& l : lines)
        program += l + " ";
    if (!program.empty()) program.pop_back();

    std::cout << "File       : " << path << "\n";
    std::cout << "Program    : " << program << "\n";
    if (debug)
        dump_source(source_builder.str());

    VarRegistry reg;

    asmjit::JitRuntime rt;
    asmjit::CodeHolder code;
    code.init(rt.environment(), rt.cpu_features());
    asmjit::StringLogger logger;
    if (debug) {
        logger.add_flags(asmjit::FormatFlags::kMachineCode |
            asmjit::FormatFlags::kHexImms |
            asmjit::FormatFlags::kHexOffsets);
        code.set_logger(&logger);
    }
    asmjit::x86::Assembler as(&code);

    auto ts = tokenize(program);
    if (debug)
        dump_tokens(ts);
    CodeGen cg(ts, as, reg);
    cg.compile();
    if (debug)
        dump_codegen(logger, code);

    RuntimeEnv env = RuntimeEnv::alloc(reg);

    JitFn fn;
    asmjit::Error err = rt.add(&fn, &code);
    if (err != asmjit::kErrorOk) {
        std::cerr << "JIT error: " << asmjit::DebugUtils::error_as_string(err) << "\n";
        env.free_all();
        return;
    }

    int64_t result = fn(&env);
    std::cout << "JIT result : " << result << "\n\n";

    rt.release(fn);
    env.free_all();
}

void run(std::initializer_list<std::string> lines, int64_t expected) {
    run(std::vector<std::string>(lines), expected);
}

void run(std::initializer_list<std::string> lines) {
    run(lines, 0);
}
