#pragma once

#include "common.h"

enum class Type { Int, Array, String };

struct BArray {
    int64_t* data;
    int64_t len;
};

struct BString {
    int8_t* data;
    int64_t len;
};

struct VarInfo {
    Type type;
    int slot;
};

struct VarRegistry {
    std::unordered_map<std::string, VarInfo> vars;
    int n_ints = 0;
    int n_arrays = 0;
    int n_strings = 0;
    std::vector<int> freeStringSlots;

    int allocStringSlot();
    void freeStringSlot(int slot);
    VarInfo declare(const std::string& name, Type type);
    VarInfo lookup(const std::string& name) const;
};

struct RuntimeEnv {
    int64_t* ints;
    BArray* arrays;
    BString* strings;
    int n_ints;
    int n_arrays;
    int n_strings;

    static RuntimeEnv alloc(const VarRegistry& reg);
    void free_all();
};

void rt_print_string(RuntimeEnv* e, int slot);
void rt_print_int(int64_t val);
void rt_array_dim(RuntimeEnv* e, int slot, int64_t len);
int64_t rt_array_get(RuntimeEnv* e, int slot, int64_t idx);
void rt_array_set(RuntimeEnv* e, int slot, int64_t idx, int64_t val);
void rt_string_free(RuntimeEnv* e, int slot);
void rt_string_assign(RuntimeEnv* e, int slot, const char* src);
char* rt_string_concat(RuntimeEnv* e, int slotA, int slotB);
void rt_string_assign_ptr(RuntimeEnv* e, int slot, char* src);
int64_t rt_string_get(RuntimeEnv* e, int slot, int64_t idx);
void rt_string_set(RuntimeEnv* e, int slot, int64_t idx, int64_t val);
