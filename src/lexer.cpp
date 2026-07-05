#include "lexer.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

namespace hc {

const char* punctStr(P p) {
    switch (p) {
        case P::None:
            return "<none>";
        case P::LParen:
            return "(";
        case P::RParen:
            return ")";
        case P::LBracket:
            return "[";
        case P::RBracket:
            return "]";
        case P::LBrace:
            return "{";
        case P::RBrace:
            return "}";
        case P::Comma:
            return ",";
        case P::Semi:
            return ";";
        case P::Colon:
            return ":";
        case P::ColonColon:
            return "::";
        case P::Dot:
            return ".";
        case P::Ellipsis:
            return "...";
        case P::Arrow:
            return "->";
        case P::Plus:
            return "+";
        case P::Minus:
            return "-";
        case P::Star:
            return "*";
        case P::Slash:
            return "/";
        case P::Percent:
            return "%";
        case P::Pow:
            return "`";
        case P::Amp:
            return "&";
        case P::Pipe:
            return "|";
        case P::Caret:
            return "^";
        case P::Shl:
            return "<<";
        case P::Shr:
            return ">>";
        case P::Tilde:
            return "~";
        case P::Not:
            return "!";
        case P::AndAnd:
            return "&&";
        case P::OrOr:
            return "||";
        case P::XorXor:
            return "^^";
        case P::Lt:
            return "<";
        case P::Gt:
            return ">";
        case P::Le:
            return "<=";
        case P::Ge:
            return ">=";
        case P::EqEq:
            return "==";
        case P::Ne:
            return "!=";
        case P::Assign:
            return "=";
        case P::PlusEq:
            return "+=";
        case P::MinusEq:
            return "-=";
        case P::StarEq:
            return "*=";
        case P::SlashEq:
            return "/=";
        case P::PercentEq:
            return "%=";
        case P::ShlEq:
            return "<<=";
        case P::ShrEq:
            return ">>=";
        case P::AmpEq:
            return "&=";
        case P::PipeEq:
            return "|=";
        case P::CaretEq:
            return "^=";
        case P::PlusPlus:
            return "++";
        case P::MinusMinus:
            return "--";
        case P::Dollar:
            return "$";
        case P::At:
            return "@";
        case P::Question:
            return "?";
    }
    return "?";
}

static std::string dirOf(const std::string& path) {
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

Lexer::Lexer(std::string path, bool jitMode, std::vector<std::string> includeDirs)
    : includeDirs_(std::move(includeDirs)), jitMode_(jitMode) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fprintf(stderr, "hcc: cannot open '%s'\n", path.c_str());
        hadError_ = true;
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    Source s;
    s.buf = ss.str();
    s.file = path;
    s.dir = dirOf(path);
    stack_.push_back(std::move(s));
}

std::unique_ptr<Lexer> Lexer::fromBuffer(std::string buf, std::string name, bool jitMode) {
    auto lx = std::unique_ptr<Lexer>(new Lexer(std::move(buf), std::move(name), jitMode, 0));
    return lx;
}

// private delegating ctor trick: define an overload used by fromBuffer
// (declared implicitly via friend-less static). We fake it with a tag param.
// -- implemented as a separate constructor below.

int Lexer::peekc(int off) {
    if (atEof()) return -1;
    const Source& s = stack_.back();
    if (s.pos + off >= s.buf.size()) return -1;
    return (unsigned char)s.buf[s.pos + off];
}

int Lexer::getc() {
    if (atEof()) return -1;
    Source& s = stack_.back();
    if (s.pos >= s.buf.size()) return -1;
    int c = (unsigned char)s.buf[s.pos++];
    if (c == '\n') {
        s.line++;
        s.col = 1;
    } else {
        s.col++;
    }
    return c;
}

void Lexer::error(const SrcLoc& loc, const std::string& msg) {
    fprintf(stderr, "%s: error: %s\n", loc.str().c_str(), msg.c_str());
    hadError_ = true;
}

SrcLoc Lexer::here() {
    SrcLoc l;
    if (!atEof()) {
        l.file = src().file;
        l.line = src().line;
        l.col = src().col;
    } else {
        l.file = "<eof>";
    }
    return l;
}

// ---------------------------------------------------------------- next()

Token Lexer::next() {
    for (;;) {
        if (!pending_.empty()) {
            Token t = pending_.front();
            pending_.pop_front();
            return expandOrReturn(std::move(t));
        }
        Token t = rawNext();
        if (t.kind == Tok::Eof) return t;
        return expandOrReturn(std::move(t));
    }
}

bool Lexer::includeFirst(const std::string& path) { return openInclude(path, here()); }

// TempleOS compiler directives implemented natively (the originals use
// #exe{StreamPrint(...)} against compiler internals).
bool Lexer::builtinMacro(Token& t) {
    const std::string& n = t.text;
    if (n == "__LINE__") {
        t.kind = Tok::IntLit;
        t.ival = t.loc.line;
        return true;
    }
    if (n == "__FILE__") {
        t.kind = Tok::StrLit;
        t.text = t.loc.file;
        return true;
    }
    if (n == "__DIR__") {
        t.kind = Tok::StrLit;
        t.text = dirOf(t.loc.file);
        return true;
    }
    if (n == "__CMD_LINE__") {
        t.kind = Tok::IntLit;
        t.ival = jitMode_ ? 1 : 0;
        return true;
    }
    if (n == "__DATE__" || n == "__TIME__") {
        time_t now = time(nullptr);
        struct tm tmv;
        localtime_r(&now, &tmv);
        char buf[32];
        if (n == "__DATE__")
            snprintf(buf, sizeof buf, "%02d/%02d/%04d", tmv.tm_mon + 1, tmv.tm_mday,
                     tmv.tm_year + 1900);
        else
            snprintf(buf, sizeof buf, "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
        t.kind = Tok::StrLit;
        t.text = buf;
        return true;
    }
    return false;
}

Token Lexer::expandOrReturn(Token t) {
    if (t.kind != Tok::Ident || t.noExpand) return t;
    if (builtinMacro(t)) return t;
    auto it = macros_.find(t.text);
    if (it == macros_.end()) return t;
    if (expanding_.count(t.text)) {  // recursion guard: leave as-is
        t.noExpand = true;
        return t;
    }
    // Substitute body; recursively expand via pending queue. To keep this
    // simple (object-like macros only), we expand bodies eagerly with a guard.
    expanding_.insert(t.text);
    std::vector<Token> out;
    std::function<void(const std::vector<Token>&)> expand = [&](const std::vector<Token>& body) {
        for (const Token& bt : body) {
            if (bt.kind == Tok::Ident && !bt.noExpand && !expanding_.count(bt.text)) {
                auto j = macros_.find(bt.text);
                if (j != macros_.end()) {
                    expanding_.insert(bt.text);
                    expand(j->second);
                    expanding_.erase(bt.text);
                    continue;
                }
            }
            Token copy = bt;
            copy.loc = t.loc;  // report at use site
            out.push_back(std::move(copy));
        }
    };
    expand(it->second);
    expanding_.erase(t.text);
    if (out.empty()) {
        // empty macro: produce next token
        if (!pending_.empty()) {
            Token n = pending_.front();
            pending_.pop_front();
            return expandOrReturn(std::move(n));
        }
        Token n = rawNext();
        return n.kind == Tok::Eof ? n : expandOrReturn(std::move(n));
    }
    for (size_t i = out.size(); i-- > 1;) pending_.push_front(out[i]);
    return out[0];
}

// ---------------------------------------------------------------- raw lexing

Token Lexer::rawNext() {
    for (;;) {
        if (atEof()) {
            Token t;
            t.kind = Tok::Eof;
            t.loc = here();
            return t;
        }
        // skip whitespace & comments
        int c = peekc();
        if (c == -1) {
            if (!conds_.empty() && stack_.size() == 1)
                error(here(), "unterminated #if/#ifdef at end of file");
            stack_.pop_back();
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
            getc();
            continue;
        }
        if (c == '/' && peekc(1) == '/') {
            while (peekc() != -1 && peekc() != '\n') getc();
            continue;
        }
        if (c == '/' && peekc(1) == '*') {
            SrcLoc l = here();
            getc();
            getc();
            for (;;) {
                if (peekc() == -1) {
                    error(l, "unterminated /* comment");
                    break;
                }
                if (peekc() == '*' && peekc(1) == '/') {
                    getc();
                    getc();
                    break;
                }
                getc();
            }
            continue;
        }
        if (c == '#') {
            getc();
            handleDirective();
            continue;
        }
        if (skippingConds()) {
            skipCondBlockLine();
            continue;
        }
        return lexToken();
    }
}

bool Lexer::skippingConds() const {
    for (const Cond& c : conds_)
        if (!c.active) return true;
    return false;
}

// While in a skipped conditional region, consume everything except
// directives (which rawNext handles) -- line by line, char by char.
void Lexer::skipCondBlockLine() {
    // consume chars up to newline or '#'
    for (;;) {
        int c = peekc();
        if (c == -1 || c == '#') return;
        if (c == '/' && peekc(1) == '/') {
            while (peekc() != -1 && peekc() != '\n') getc();
            continue;
        }
        if (c == '/' && peekc(1) == '*') {
            getc();
            getc();
            while (peekc() != -1 && !(peekc() == '*' && peekc(1) == '/')) getc();
            if (peekc() != -1) {
                getc();
                getc();
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            Token dummy;
            lexCharsInto(dummy, (char)c);
            continue;
        }
        getc();
        if (c == '\n') return;
    }
}

void Lexer::skipToEndOfLine(std::string* capture) {
    for (;;) {
        int c = peekc();
        if (c == -1) return;
        if (c == '\\' && peekc(1) == '\n') {  // line splice
            getc();
            getc();
            if (capture) capture->push_back('\n');
            continue;
        }
        if (c == '\n') return;
        getc();
        if (capture) capture->push_back((char)c);
    }
}

std::vector<Token> Lexer::lexLineTokens() {
    std::vector<Token> out;
    int startLine = atEof() ? -1 : src().line;
    for (;;) {
        if (atEof()) break;
        // stop at end of (possibly spliced) line
        int c = peekc();
        while (c == ' ' || c == '\t' || c == '\r') {
            getc();
            c = peekc();
        }
        if (c == '\\' && peekc(1) == '\n') {
            getc();
            getc();
            startLine = src().line;
            continue;
        }
        if (c == -1 || c == '\n') break;
        if (c == '/' && peekc(1) == '/') {
            skipToEndOfLine();
            break;
        }
        if (c == '/' && peekc(1) == '*') {
            getc();
            getc();
            while (peekc() != -1 && !(peekc() == '*' && peekc(1) == '/')) getc();
            if (peekc() != -1) {
                getc();
                getc();
            }
            continue;
        }
        if (src().line != startLine) break;
        out.push_back(lexToken());
    }
    return out;
}

// ---------------------------------------------------------------- directives

static bool tokIsInt(const Token& t) { return t.kind == Tok::IntLit || t.kind == Tok::CharLit; }

void Lexer::handleDirective() {
    SrcLoc loc = here();
    // read directive name
    std::string name;
    while (isalnum(peekc()) || peekc() == '_') name.push_back((char)getc());

    bool skipping = skippingConds();

    if (name == "ifdef" || name == "ifndef") {
        auto toks = lexLineTokens();
        bool val = false;
        if (toks.size() == 1 && toks[0].kind == Tok::Ident)
            val = macros_.count(toks[0].text) != 0;
        else if (!skipping)
            error(loc, "#" + name + " expects a single identifier");
        if (name == "ifndef") val = !val;
        conds_.push_back({skipping ? false : val, skipping ? true : val, false});
        return;
    }
    if (name == "if") {
        auto toks = lexLineTokens();
        bool val = skipping ? false : evalCondExpr(std::move(toks), loc) != 0;
        conds_.push_back({val, val, false});
        return;
    }
    if (name == "ifaot" || name == "ifjit") {
        lexLineTokens();
        bool val = (name == "ifjit") == jitMode_;
        if (skipping) val = false;
        conds_.push_back({val, val, false});
        return;
    }
    if (name == "else") {
        lexLineTokens();
        if (conds_.empty()) {
            error(loc, "#else without #if");
            return;
        }
        Cond& c = conds_.back();
        if (c.elseSeen) error(loc, "duplicate #else");
        c.elseSeen = true;
        bool outerActive = true;
        for (size_t i = 0; i + 1 < conds_.size(); i++) outerActive &= conds_[i].active;
        c.active = outerActive && !c.taken;
        c.taken = true;
        return;
    }
    if (name == "endif") {
        lexLineTokens();
        if (conds_.empty())
            error(loc, "#endif without #if");
        else
            conds_.pop_back();
        return;
    }

    if (skipping) {  // other directives are inert inside skipped regions
        skipToEndOfLine();
        return;
    }

    if (name == "define") {
        auto toks = lexLineTokens();
        if (toks.empty() || toks[0].kind != Tok::Ident) {
            error(loc, "#define expects an identifier");
            return;
        }
        // "No #define functions exist (I'm not a fan)"
        if (toks.size() > 1 && toks[1].is(P::LParen) &&
            toks[1].loc.col == toks[0].loc.col + (int)toks[0].text.size())
            error(loc, "no #define functions exist in HolyC");
        std::string mname = toks[0].text;
        toks.erase(toks.begin());
        macros_[mname] = std::move(toks);
        return;
    }
    if (name == "undef") {
        auto toks = lexLineTokens();
        if (toks.size() == 1 && toks[0].kind == Tok::Ident)
            macros_.erase(toks[0].text);
        else
            error(loc, "#undef expects a single identifier");
        return;
    }
    if (name == "include") {
        auto toks = lexLineTokens();
        if (toks.size() == 1 && toks[0].kind == Tok::StrLit) {
            if (!openInclude(toks[0].text, loc))
                error(loc, "cannot open include file \"" + toks[0].text + "\"");
        } else {
            // Can't use <> with #include, use "".
            error(loc, "#include expects a \"filename\" (no <> form in HolyC)");
        }
        return;
    }
    if (name == "assert") {
        auto toks = lexLineTokens();
        int64_t v = evalCondExpr(toks, loc);
        if (!v) fprintf(stderr, "%s: warning: #assert failed\n", loc.str().c_str());
        return;
    }
    if (name == "help_index" || name == "help_file") {
        skipToEndOfLine();
        return;
    }
    if (name == "exe") {
        handleExeBlock();
        return;
    }
    error(loc, "unknown directive #" + name);
    skipToEndOfLine();
}

void Lexer::handleExeBlock() {
    SrcLoc loc = here();
    // expect '{' ... matching '}'
    int c;
    do {
        c = getc();
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\n');
    if (c != '{') {
        error(loc, "#exe expects '{'");
        return;
    }
    std::string body;
    int depth = 1;
    for (;;) {
        c = peekc();
        if (c == -1) {
            error(loc, "unterminated #exe{}");
            return;
        }
        if (c == '"' || c == '\'') {
            size_t start = src().pos;
            Token dummy;
            lexCharsInto(dummy, (char)getc());
            body.append(src().buf, start, src().pos - start);
            continue;
        }
        if (c == '/' && peekc(1) == '/') {
            while (peekc() != -1 && peekc() != '\n') body.push_back((char)getc());
            continue;
        }
        if (c == '{') depth++;
        if (c == '}') {
            if (--depth == 0) {
                getc();
                break;
            }
        }
        body.push_back((char)getc());
    }
    if (!exeHook_) {
        error(loc, "#exe{} is not available in this compilation mode");
        return;
    }
    std::string inject = exeHook_(body, loc);
    if (!inject.empty()) {
        // Splice injected text as a new source on top of the stack.
        Source s;
        s.buf = std::move(inject);
        s.file = loc.file + ":<#exe>";
        s.dir = atEof() ? std::string(".") : src().dir;
        stack_.push_back(std::move(s));
    }
}

bool Lexer::openInclude(const std::string& name, const SrcLoc& loc) {
    std::vector<std::string> tries;
    if (!name.empty() && name[0] == '/') {
        tries.push_back(name);
    } else {
        if (!atEof()) tries.push_back(src().dir + "/" + name);
        for (auto& d : includeDirs_) tries.push_back(d + "/" + name);
    }
    for (auto& path : tries) {
        std::ifstream f(path, std::ios::binary);
        if (!f) continue;
        std::stringstream ss;
        ss << f.rdbuf();
        Source s;
        s.buf = ss.str();
        s.file = path;
        s.dir = dirOf(path);
        stack_.push_back(std::move(s));
        return true;
    }
    return false;
}

// Tiny constant-expression evaluator for #if / #assert.
// Uses HolyC precedence. Supports defined(X), integers, ! ~ - unary, && || etc.
namespace {
struct CondEval {
    const std::vector<Token>& t;
    size_t i = 0;
    Lexer* lx;
    std::map<std::string, std::vector<Token>>& macros;
    bool err = false;

    const Token* peek() { return i < t.size() ? &t[i] : nullptr; }
    bool eatP(P p) {
        if (i < t.size() && t[i].is(p)) {
            i++;
            return true;
        }
        return false;
    }
    int64_t primary() {
        if (i >= t.size()) {
            err = true;
            return 0;
        }
        const Token& tk = t[i];
        if (tk.kind == Tok::IntLit || tk.kind == Tok::CharLit) {
            i++;
            return tk.ival;
        }
        if (tk.kind == Tok::FloatLit) {
            i++;
            return (int64_t)tk.fval;
        }
        if (tk.kind == Tok::Ident && tk.text == "defined") {
            i++;
            bool paren = eatP(P::LParen);
            if (i < t.size() && t[i].kind == Tok::Ident) {
                int64_t v = macros.count(t[i].text) ? 1 : 0;
                i++;
                if (paren && !eatP(P::RParen)) err = true;
                return v;
            }
            err = true;
            return 0;
        }
        if (tk.kind == Tok::Ident) {
            // expand macro if object-like const; unknown identifiers are 0
            auto it = macros.find(tk.text);
            i++;
            if (it != macros.end() && it->second.size() == 1 && tokIsInt(it->second[0]))
                return it->second[0].ival;
            if (it != macros.end() && !it->second.empty()) {
                CondEval sub{it->second, 0, lx, macros};
                return sub.expr();
            }
            return 0;
        }
        if (eatP(P::LParen)) {
            int64_t v = expr();
            if (!eatP(P::RParen)) err = true;
            return v;
        }
        err = true;
        return 0;
    }
    int64_t unary() {
        if (eatP(P::Minus)) return -unary();
        if (eatP(P::Plus)) return unary();
        if (eatP(P::Not)) return !unary();
        if (eatP(P::Tilde)) return ~unary();
        return primary();
    }
    // HolyC precedence tiers (subset for consts)
    int64_t tier1() {  // ` << >>
        int64_t v = unary();
        for (;;) {
            if (eatP(P::Shl))
                v <<= unary();
            else if (eatP(P::Shr))
                v >>= unary();
            else if (eatP(P::Pow)) {
                int64_t e = unary();
                int64_t r = 1;
                while (e-- > 0) r *= v;
                v = r;
            } else
                return v;
        }
    }
    int64_t tier2() {
        int64_t v = tier1();
        for (;;) {
            if (eatP(P::Star))
                v *= tier1();
            else if (eatP(P::Slash)) {
                int64_t d = tier1();
                v = d ? v / d : (err = true, 0);
            } else if (eatP(P::Percent)) {
                int64_t d = tier1();
                v = d ? v % d : (err = true, 0);
            } else
                return v;
        }
    }
    int64_t tierAnd() {
        int64_t v = tier2();
        while (eatP(P::Amp)) v &= tier2();
        return v;
    }
    int64_t tierXor() {
        int64_t v = tierAnd();
        while (eatP(P::Caret)) v ^= tierAnd();
        return v;
    }
    int64_t tierOr() {
        int64_t v = tierXor();
        while (eatP(P::Pipe)) v |= tierXor();
        return v;
    }
    int64_t tierAdd() {
        int64_t v = tierOr();
        for (;;) {
            if (eatP(P::Plus))
                v += tierOr();
            else if (eatP(P::Minus))
                v -= tierOr();
            else
                return v;
        }
    }
    int64_t tierRel() {
        int64_t v = tierAdd();
        for (;;) {
            if (eatP(P::Lt))
                v = v < tierAdd();
            else if (eatP(P::Gt))
                v = v > tierAdd();
            else if (eatP(P::Le))
                v = v <= tierAdd();
            else if (eatP(P::Ge))
                v = v >= tierAdd();
            else
                return v;
        }
    }
    int64_t tierEq() {
        int64_t v = tierRel();
        for (;;) {
            if (eatP(P::EqEq))
                v = v == tierRel();
            else if (eatP(P::Ne))
                v = v != tierRel();
            else
                return v;
        }
    }
    int64_t tierLAnd() {
        int64_t v = tierEq();
        while (eatP(P::AndAnd)) {
            int64_t r = tierEq();
            v = v && r;
        }
        return v;
    }
    int64_t tierLXor() {
        int64_t v = tierLAnd();
        while (eatP(P::XorXor)) {
            int64_t r = tierLAnd();
            v = (!!v) ^ (!!r);
        }
        return v;
    }
    int64_t expr() {
        int64_t v = tierLXor();
        while (eatP(P::OrOr)) {
            int64_t r = tierLXor();
            v = v || r;
        }
        return v;
    }
};
}  // namespace

int64_t Lexer::evalCondExpr(std::vector<Token> toks, const SrcLoc& loc) {
    if (toks.empty()) {
        error(loc, "empty preprocessor expression");
        return 0;
    }
    CondEval ev{toks, 0, this, macros_};
    int64_t v = ev.expr();
    if (ev.err || ev.i != toks.size()) error(loc, "bad preprocessor expression");
    return v;
}

// ---------------------------------------------------------------- tokens

// decode chars between quotes into t.text (handles escapes and $$)
void Lexer::lexCharsInto(Token& t, char quote) {
    SrcLoc loc = here();
    for (;;) {
        int c = getc();
        if (c == -1) {
            error(loc, "unterminated literal");
            return;
        }
        if (c == quote) return;
        if (c == '$') {
            // "$" is an escape character. Two dollar signs signify an ordinary $.
            if (peekc() == '$') {
                getc();
                t.text.push_back('$');
            } else {
                // Lone $: DolDoc command text; pass through verbatim in hosted impl.
                t.text.push_back('$');
            }
            continue;
        }
        if (c != '\\') {
            t.text.push_back((char)c);
            continue;
        }
        int e = getc();
        switch (e) {
            case 'n':
                t.text.push_back('\n');
                break;
            case 't':
                t.text.push_back('\t');
                break;
            case 'r':
                t.text.push_back('\r');
                break;
            case 'a':
                t.text.push_back('\a');
                break;
            case 'b':
                t.text.push_back('\b');
                break;
            case 'f':
                t.text.push_back('\f');
                break;
            case 'v':
                t.text.push_back('\v');
                break;
            case '0':
                t.text.push_back('\0');
                break;
            case '\\':
                t.text.push_back('\\');
                break;
            case '\'':
                t.text.push_back('\'');
                break;
            case '"':
                t.text.push_back('"');
                break;
            case 'x': {
                int v = 0, n = 0;
                while (n < 2 && isxdigit(peekc())) {
                    int d = getc();
                    v = v * 16 + (isdigit(d) ? d - '0' : (tolower(d) - 'a' + 10));
                    n++;
                }
                if (!n) error(loc, "\\x expects hex digits");
                t.text.push_back((char)v);
                break;
            }
            case -1:
                error(loc, "unterminated literal");
                return;
            default:
                t.text.push_back((char)e);
                break;
        }
    }
}

Token Lexer::lexNumber() {
    Token t;
    t.loc = here();
    std::string num;
    bool isFloat = false;
    int c = peekc();
    if (c == '0' && (peekc(1) == 'x' || peekc(1) == 'X')) {
        getc();
        getc();
        while (isxdigit(peekc()) || peekc() == '_') {
            int d = getc();
            if (d != '_') num.push_back((char)d);
        }
        t.kind = Tok::IntLit;
        t.ival = (int64_t)strtoull(num.c_str(), nullptr, 16);
        return t;
    }
    if (c == '0' && (peekc(1) == 'b' || peekc(1) == 'B')) {
        getc();
        getc();
        while (peekc() == '0' || peekc() == '1' || peekc() == '_') {
            int d = getc();
            if (d != '_') num.push_back((char)d);
        }
        t.kind = Tok::IntLit;
        t.ival = (int64_t)strtoull(num.c_str(), nullptr, 2);
        return t;
    }
    while (isdigit(peekc()) || peekc() == '_') {
        int d = getc();
        if (d != '_') num.push_back((char)d);
    }
    if (peekc() == '.' && isdigit(peekc(1))) {
        isFloat = true;
        num.push_back((char)getc());
        while (isdigit(peekc()) || peekc() == '_') {
            int d = getc();
            if (d != '_') num.push_back((char)d);
        }
    }
    if ((peekc() == 'e' || peekc() == 'E') &&
        (isdigit(peekc(1)) || ((peekc(1) == '+' || peekc(1) == '-') && isdigit(peekc(2))))) {
        isFloat = true;
        num.push_back((char)getc());
        num.push_back((char)getc());
        while (isdigit(peekc())) num.push_back((char)getc());
    }
    if (isFloat) {
        t.kind = Tok::FloatLit;
        t.fval = strtod(num.c_str(), nullptr);
    } else {
        t.kind = Tok::IntLit;
        t.ival = (int64_t)strtoull(num.c_str(), nullptr, 10);
    }
    return t;
}

Token Lexer::lexToken() {
    Token t;
    t.loc = here();
    int c = peekc();

    if (isalpha(c) || c == '_' || c == '@') {
        // '@' allowed to start asm-local labels; keep as punct otherwise.
        if (c == '@' && peekc(1) != '@') {
            getc();
            t.kind = Tok::Punct;
            t.punct = P::At;
            return t;
        }
        t.kind = Tok::Ident;
        while (isalnum(peekc()) || peekc() == '_' || peekc() == '@') t.text.push_back((char)getc());
        return t;
    }
    if (isdigit(c)) return lexNumber();
    if (c == '"') {
        getc();
        t.kind = Tok::StrLit;
        lexCharsInto(t, '"');
        return t;
    }
    if (c == '\'') {
        getc();
        t.kind = Tok::CharLit;
        lexCharsInto(t, '\'');
        if (t.text.size() > 8) error(t.loc, "char constant longer than 8 characters");
        uint64_t v = 0;
        for (size_t i = t.text.size(); i-- > 0;) v = (v << 8) | (uint8_t)t.text[i];
        t.ival = (int64_t)v;
        return t;
    }

    t.kind = Tok::Punct;
    getc();
    auto two = [&](char n, P yes, P no) {
        if (peekc() == n) {
            getc();
            t.punct = yes;
        } else
            t.punct = no;
    };
    switch (c) {
        case '(':
            t.punct = P::LParen;
            break;
        case ')':
            t.punct = P::RParen;
            break;
        case '[':
            t.punct = P::LBracket;
            break;
        case ']':
            t.punct = P::RBracket;
            break;
        case '{':
            t.punct = P::LBrace;
            break;
        case '}':
            t.punct = P::RBrace;
            break;
        case ',':
            t.punct = P::Comma;
            break;
        case ';':
            t.punct = P::Semi;
            break;
        case '?':
            t.punct = P::Question;
            break;
        case '$':
            t.punct = P::Dollar;
            break;
        case '`':
            t.punct = P::Pow;
            break;
        case '~':
            t.punct = P::Tilde;
            break;
        case ':':
            two(':', P::ColonColon, P::Colon);
            break;
        case '.':
            if (peekc() == '.' && peekc(1) == '.') {
                getc();
                getc();
                t.punct = P::Ellipsis;
            } else
                t.punct = P::Dot;
            break;
        case '+':
            if (peekc() == '+') {
                getc();
                t.punct = P::PlusPlus;
            } else
                two('=', P::PlusEq, P::Plus);
            break;
        case '-':
            if (peekc() == '-') {
                getc();
                t.punct = P::MinusMinus;
            } else if (peekc() == '>') {
                getc();
                t.punct = P::Arrow;
            } else
                two('=', P::MinusEq, P::Minus);
            break;
        case '*':
            two('=', P::StarEq, P::Star);
            break;
        case '/':
            two('=', P::SlashEq, P::Slash);
            break;
        case '%':
            two('=', P::PercentEq, P::Percent);
            break;
        case '!':
            two('=', P::Ne, P::Not);
            break;
        case '=':
            two('=', P::EqEq, P::Assign);
            break;
        case '&':
            if (peekc() == '&') {
                getc();
                t.punct = P::AndAnd;
            } else
                two('=', P::AmpEq, P::Amp);
            break;
        case '|':
            if (peekc() == '|') {
                getc();
                t.punct = P::OrOr;
            } else
                two('=', P::PipeEq, P::Pipe);
            break;
        case '^':
            if (peekc() == '^') {
                getc();
                t.punct = P::XorXor;
            } else
                two('=', P::CaretEq, P::Caret);
            break;
        case '<':
            if (peekc() == '<') {
                getc();
                two('=', P::ShlEq, P::Shl);
            } else
                two('=', P::Le, P::Lt);
            break;
        case '>':
            if (peekc() == '>') {
                getc();
                two('=', P::ShrEq, P::Shr);
            } else
                two('=', P::Ge, P::Gt);
            break;
        default:
            error(t.loc, std::string("stray character '") + (char)c + "'");
            return rawNext();
    }
    return t;
}

// buffer constructor (tag overload)
Lexer::Lexer(std::string buf, std::string name, bool jitMode, int /*tag*/) : jitMode_(jitMode) {
    Source s;
    s.buf = std::move(buf);
    s.file = std::move(name);
    s.dir = ".";
    stack_.push_back(std::move(s));
}

}  // namespace hc
