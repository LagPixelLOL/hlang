// hlang -- HolyC parser.
#pragma once
#include <memory>
#include <string>

#include "ast.hpp"
#include "lexer.hpp"

namespace hc {

class Parser {
public:
    explicit Parser(Lexer& lx);
    std::unique_ptr<Program> parseProgram();
    bool hadError() const { return hadError_; }

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    bool hadError_ = false;
    friend struct Impl;
};

// s-expression dump for tests (--dump-ast)
std::string dumpProgram(const Program& p);

}  // namespace hc
