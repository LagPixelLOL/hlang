// hlang -- HolyC lexer with built-in preprocessor.
// "There is no separate preprocessor pass. The parser front-end calls Lex()
//  which has the preprocessor built-in."  -- TempleOS docs
#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace hc {

enum class Tok {
    Eof,
    Ident,
    IntLit,    // I64 value (also multi-char char consts)
    FloatLit,  // F64 value
    StrLit,    // decoded bytes in text
    CharLit,   // multi-char char constant; ival holds packed value; text holds raw chars ("" =>
               // empty '' literal)
    Punct,     // punctuator/operator id in `punct`
};

enum class P {
    None,
    LParen,
    RParen,
    LBracket,
    RBracket,
    LBrace,
    RBrace,
    Comma,
    Semi,
    Colon,
    ColonColon,
    Dot,
    Ellipsis,
    Arrow,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Pow /* ` */,
    Amp,
    Pipe,
    Caret,
    Shl,
    Shr,
    Tilde,
    Not,
    AndAnd,
    OrOr,
    XorXor,
    Lt,
    Gt,
    Le,
    Ge,
    EqEq,
    Ne,
    Assign,
    PlusEq,
    MinusEq,
    StarEq,
    SlashEq,
    PercentEq,
    ShlEq,
    ShrEq,
    AmpEq,
    PipeEq,
    CaretEq,
    PlusPlus,
    MinusMinus,
    Dollar,
    At,
    Question,
};

struct SrcLoc {
    std::string file;
    int line = 0;
    int col = 0;
    std::string str() const {
        return file + ":" + std::to_string(line) + ":" + std::to_string(col);
    }
};

struct Token {
    Tok kind = Tok::Eof;
    P punct = P::None;
    std::string text;  // ident name / decoded string bytes / raw char-lit chars
    int64_t ival = 0;
    double fval = 0;
    SrcLoc loc;
    bool noExpand = false;  // macro recursion guard

    bool is(P p) const { return kind == Tok::Punct && punct == p; }
    bool isIdent(const char* s) const { return kind == Tok::Ident && text == s; }
};

const char* punctStr(P p);

// Compile-time #exe{} hook: receives HolyC source of the block, returns the
// text to inject into the stream (StreamPrint output).
using ExeHook = std::function<std::string(const std::string& src, const SrcLoc& loc)>;

class Lexer {
public:
    // jitMode selects #ifjit/#ifaot branches.
    Lexer(std::string path, bool jitMode, std::vector<std::string> includeDirs);
    // For in-memory buffers (tests, #exe).
    static std::unique_ptr<Lexer> fromBuffer(std::string buf, std::string name, bool jitMode);

    Token next();

    // Push a file to be lexed before the main file (stdlib prelude).
    bool includeFirst(const std::string& path);

    bool hadError() const { return hadError_; }
    void setExeHook(ExeHook h) { exeHook_ = std::move(h); }
    void addDefine(const std::string& name, std::vector<Token> body) {
        macros_[name] = std::move(body);
    }

private:
    Lexer(std::string buf, std::string name, bool jitMode, int tag);  // in-memory buffer

    struct Source {
        std::string buf;
        size_t pos = 0;
        std::string file;  // full path (or <name>)
        std::string dir;   // directory of file, for relative includes
        int line = 1;
        int col = 1;
    };

    std::vector<Source> stack_;
    std::deque<Token> pending_;
    std::map<std::string, std::vector<Token>> macros_;
    std::set<std::string> expanding_;
    std::vector<std::string> includeDirs_;
    bool jitMode_ = true;
    bool hadError_ = false;
    ExeHook exeHook_;

    // conditional inclusion stack: state of each nested #if
    struct Cond {
        bool active;
        bool taken;
        bool elseSeen;
    };
    std::vector<Cond> conds_;

    Source& src() { return stack_.back(); }
    bool atEof() const { return stack_.empty(); }
    int peekc(int off = 0);
    int getc();
    void error(const SrcLoc& loc, const std::string& msg);
    SrcLoc here();

    Token rawNext();   // one token, no macro expansion, handles directives
    Token lexToken();  // one token from chars (no directive handling)
    void handleDirective();
    void skipToEndOfLine(std::string* capture = nullptr);
    std::vector<Token> lexLineTokens();  // rest of current line, raw tokens
    bool skippingConds() const;
    void skipCondBlockLine();
    int64_t evalCondExpr(std::vector<Token> toks, const SrcLoc& loc);
    bool openInclude(const std::string& name, const SrcLoc& loc);
    void lexCharsInto(Token& t, char quote);
    Token lexNumber();
    Token expandOrReturn(Token t);  // apply macro expansion
    bool builtinMacro(Token& t);    // __DATE__, __FILE__, __LINE__, ...
    void handleExeBlock();
};

}  // namespace hc
