#pragma once

#include "codegen.h"

void run(const std::vector<std::string>& lines, int64_t expected);
void run(std::initializer_list<std::string> lines, int64_t expected);
void run(std::initializer_list<std::string> lines);
void run_file(const std::string& path, bool debug = false);
