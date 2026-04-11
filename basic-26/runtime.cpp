#include "runtime.h"

int VarRegistry::allocStringSlot() {
    if (!freeStringSlots.empty()) {
        int s = freeStringSlots.back();
        freeStringSlots.pop_back();
        return s;
    }
    return n_strings++;
}

void VarRegistry::freeStringSlot(int slot) {
    freeStringSlots.push_back(slot);
}

VarInfo VarRegistry::declare(const std::string& name, Type type) {
    auto [it, inserted] = vars.emplace(name, VarInfo{});
    if (inserted) {
        int slot = (type == Type::Int) ? n_ints++
            : (type == Type::Array) ? n_arrays++
            : n_strings++;
        it->second = { type, slot };
    }
    return it->second;
}

VarInfo VarRegistry::lookup(const std::string& name) const {
    auto it = vars.find(name);
    if (it == vars.end()) {
        throw std::runtime_error("Undeclared variable: " + name);
    }
    return it->second;
}

RuntimeEnv RuntimeEnv::alloc(const VarRegistry& reg) {
    RuntimeEnv e;
    e.n_ints = reg.n_ints;
    e.n_arrays = reg.n_arrays;
    e.n_strings = reg.n_strings;
    e.ints = (int64_t*)calloc(e.n_ints, sizeof(int64_t));
    e.arrays = (BArray*)calloc(e.n_arrays, sizeof(BArray));
    e.strings = (BString*)calloc(e.n_strings, sizeof(BString));
    return e;
}

void RuntimeEnv::free_all() {
    for (int i = 0; i < n_arrays; ++i) {
        free(arrays[i].data);
    }
    for (int i = 0; i < n_strings; ++i) {
        free(strings[i].data);
    }
    free(ints);
    free(arrays);
    free(strings);
}

void rt_print_string(RuntimeEnv* e, int slot) {
    assert(slot < e->n_strings);
    BString& s = e->strings[slot];
    if (s.data && s.len > 0) {
        fwrite(s.data, 1, (size_t)s.len, stdout);
    }
    fputc('\n', stdout);
}

void rt_print_int(int64_t val) {
    printf("%" PRId64 "\n", val);
}

void rt_array_dim(RuntimeEnv* e, int slot, int64_t len) {
    assert(slot < e->n_arrays);
    free(e->arrays[slot].data);
    e->arrays[slot].data = (int64_t*)calloc(len, sizeof(int64_t));
    if (!e->arrays[slot].data) {
        throw std::bad_alloc{};
    }
    e->arrays[slot].len = len;
}

int64_t rt_array_get(RuntimeEnv* e, int slot, int64_t idx) {
    assert(slot < e->n_arrays);
    const auto& a = e->arrays[slot];
    if ((uint64_t)idx >= (uint64_t)a.len) {
        throw std::runtime_error("Array index out of bounds");
    }
    return a.data[idx];
}

void rt_array_set(RuntimeEnv* e, int slot, int64_t idx, int64_t val) {
    assert(slot < e->n_arrays);
    auto& a = e->arrays[slot];
    if ((uint64_t)idx >= (uint64_t)a.len) {
        throw std::runtime_error("Array index out of bounds");
    }
    a.data[idx] = val;
}

void rt_string_free(RuntimeEnv* e, int slot) {
    assert(slot < e->n_strings);
    free(e->strings[slot].data);
    e->strings[slot].data = nullptr;
    e->strings[slot].len = 0;
}

void rt_string_assign(RuntimeEnv* e, int slot, const char* src) {
    assert(slot < e->n_strings);
    int64_t len = (int64_t)strlen(src);
    free(e->strings[slot].data);
    e->strings[slot].data = (int8_t*)malloc(len + 1);
    if (!e->strings[slot].data) {
        throw std::bad_alloc{};
    }
    memcpy(e->strings[slot].data, src, len + 1);
    e->strings[slot].len = len;
}

char* rt_string_concat(RuntimeEnv* e, int slotA, int slotB) {
    const auto& a = e->strings[slotA];
    const auto& b = e->strings[slotB];
    int64_t len = a.len + b.len;
    char* buf = (char*)malloc(len + 1);
    if (!buf) {
        throw std::bad_alloc{};
    }
    memcpy(buf, a.data, a.len);
    memcpy(buf + a.len, b.data, b.len);
    buf[len] = '\0';
    return buf;
}

void rt_string_assign_ptr(RuntimeEnv* e, int slot, char* src) {
    assert(slot < e->n_strings);
    free(e->strings[slot].data);
    int64_t len = (int64_t)strlen(src);
    e->strings[slot].data = (int8_t*)src;
    e->strings[slot].len = len;
}

int64_t rt_string_get(RuntimeEnv* e, int slot, int64_t idx) {
    assert(slot < e->n_strings);
    const auto& s = e->strings[slot];
    if ((uint64_t)idx >= (uint64_t)s.len) {
        throw std::runtime_error("String index out of bounds");
    }
    return (int64_t)(uint8_t)s.data[idx];
}

void rt_string_set(RuntimeEnv* e, int slot, int64_t idx, int64_t val) {
    assert(slot < e->n_strings);
    auto& s = e->strings[slot];
    if ((uint64_t)idx >= (uint64_t)s.len) {
        throw std::runtime_error("String index out of bounds");
    }
    s.data[idx] = (int8_t)(val & 0xFF);
}
