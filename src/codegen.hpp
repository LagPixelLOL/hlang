// hlang -- LLVM IR generation from the HolyC AST.
#pragma once
#include <memory>
#include <string>

#include "ast.hpp"

namespace llvm {
class Module;
class LLVMContext;
}  // namespace llvm

namespace hc {

struct CodegenResult {
    std::unique_ptr<llvm::LLVMContext> ctx;
    std::unique_ptr<llvm::Module> module;
    bool ok = false;
};

// aotMode: also emit main() wrapper calling HC_RtInit + __HC_startup.
CodegenResult codegen(Program& prog, const std::string& moduleName, bool aotMode);

}  // namespace hc
