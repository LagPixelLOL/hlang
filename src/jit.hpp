// hlang -- ORC JIT execution + #exe{} snippet running.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "codegen.hpp"

namespace hc {

// JIT-run a module's __HC_startup. Returns process-style exit code.
// optLevel (0..3) sets the JIT backend codegen opt level.
int runJIT(CodegenResult cg, int64_t argc, char** argv, int optLevel, bool noRun = false);

// Compile and run a #exe{} snippet; returns StreamPrint()ed text.
// preludePath may be empty. Sets *ok=false on failure.
std::string runExeSnippet(const std::string& src, const std::string& name,
                          const std::string& preludePath,
                          const std::vector<std::string>& includeDirs, bool* ok);

}  // namespace hc
