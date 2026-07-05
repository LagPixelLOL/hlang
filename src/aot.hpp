// hlang -- AOT compilation: optimize, emit objects, link executables.
#pragma once
#include <string>

namespace llvm {
class Module;
}

namespace hc {

// Run the standard LLVM optimization pipeline (O0..O3).
void optimizeModule(llvm::Module& m, int level);

// Emit a native object file. Returns false (with message on stderr) on error.
bool emitObjectFile(llvm::Module& m, const std::string& path);

// Link objPath + runtime archive into an executable using the system cc.
bool linkExecutable(const std::string& objPath, const std::string& outPath,
                    const std::string& runtimeArchive);

}  // namespace hc
