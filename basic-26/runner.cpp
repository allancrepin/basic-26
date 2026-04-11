#include "runner.h"

using JitFn = int64_t(*)(RuntimeEnv*);

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

void run_file(const std::string& path) {

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
    std::string line;
    while (std::getline(f, line))
        if (!line.empty())
            lines.push_back(line);

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
