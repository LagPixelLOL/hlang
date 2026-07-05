// hlang -- HolyC parser. Builds the AST; symbol/type resolution of
// expressions happens in codegen (HolyC needs class names at parse time
// only to disambiguate declarations and postfix casts).
#include "parser.hpp"

#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <set>

namespace hc {

namespace {

struct BuiltinTy {
    int size;
    bool uns;
    bool isVoid;
    bool isF64;
};

const std::map<std::string, BuiltinTy>& builtinTypes() {
    static const std::map<std::string, BuiltinTy> m = {
        {"U0", {0, true, true, false}},
        {"I0", {0, false, true, false}},
        {"I8", {1, false, false, false}},
        {"U8", {1, true, false, false}},
        {"I16", {2, false, false, false}},
        {"U16", {2, true, false, false}},
        {"I32", {4, false, false, false}},
        {"U32", {4, true, false, false}},
        {"I64", {8, false, false, false}},
        {"U64", {8, true, false, false}},
        {"F64", {8, false, false, true}},
        // intrinsic ("i") forms: same machine types, no sub-int unions
        {"I8i", {1, false, false, false}},
        {"U8i", {1, true, false, false}},
        {"I16i", {2, false, false, false}},
        {"U16i", {2, true, false, false}},
        {"I32i", {4, false, false, false}},
        {"U32i", {4, true, false, false}},
        {"I64i", {8, false, false, false}},
        {"U64i", {8, true, false, false}},
    };
    return m;
}

bool isQualKw(const Token& t) {
    return t.kind == Tok::Ident &&
           (t.text == "public" || t.text == "static" || t.text == "extern" || t.text == "import" ||
            t.text == "_extern" || t.text == "_import" || t.text == "interrupt" ||
            t.text == "haserrcode" || t.text == "argpop" || t.text == "noargpop");
}

}  // namespace

struct Parser::Impl {
    Lexer& lx;
    std::deque<Token> buf;
    bool hadError = false;
    std::unique_ptr<Program> prog;

    explicit Impl(Lexer& l) : lx(l) { prog = std::make_unique<Program>(); }

    // ------------------------------------------------------------ tokens
    const Token& tok(size_t k = 0) {
        while (buf.size() <= k) buf.push_back(lx.next());
        return buf[k];
    }
    Token take() {
        Token t = tok();
        buf.pop_front();
        return t;
    }
    bool is(P p, size_t k = 0) { return tok(k).is(p); }
    bool isIdent(const char* s, size_t k = 0) { return tok(k).isIdent(s); }
    bool accept(P p) {
        if (is(p)) {
            take();
            return true;
        }
        return false;
    }
    void error(const SrcLoc& loc, const std::string& msg) {
        fprintf(stderr, "%s: error: %s\n", loc.str().c_str(), msg.c_str());
        hadError = true;
    }
    void expect(P p, const char* what) {
        if (!accept(p)) {
            error(tok().loc, std::string("expected '") + punctStr(p) + "' " + what + " (got '" +
                                 describe(tok()) + "')");
            // best-effort resync: skip one token
            if (tok().kind != Tok::Eof) take();
        }
    }
    static std::string describe(const Token& t) {
        switch (t.kind) {
            case Tok::Eof:
                return "<eof>";
            case Tok::Ident:
                return t.text;
            case Tok::IntLit:
                return std::to_string(t.ival);
            case Tok::FloatLit:
                return std::to_string(t.fval);
            case Tok::StrLit:
                return "\"" + t.text + "\"";
            case Tok::CharLit:
                return "'" + t.text + "'";
            case Tok::Punct:
                return punctStr(t.punct);
        }
        return "?";
    }

    // ------------------------------------------------------------ types
    bool isTypeName(const Token& t) {
        if (t.kind != Tok::Ident) return false;
        if (builtinTypes().count(t.text)) return true;
        return prog->classes.count(t.text) != 0;
    }

    TypePtr baseTypeFor(const std::string& name) {
        auto bi = builtinTypes().find(name);
        if (bi != builtinTypes().end()) {
            const BuiltinTy& b = bi->second;
            if (b.isVoid) return tyVoid();
            if (b.isF64) return tyF64();
            return tyInt(b.size, b.uns);
        }
        auto ci = prog->classes.find(name);
        if (ci != prog->classes.end()) {
            // Typed unions ("I64i union I64") are used whole via wholeType,
            // but keep the class type so sub-member access still works.
            Type t(Type::Class);
            t.cls = ci->second;
            return std::make_shared<Type>(t);
        }
        return nullptr;
    }

    // parse: BASETYPE '*'*  (the common prefix of declarations and casts)
    TypePtr parseBaseType() {
        Token t = take();
        TypePtr ty = baseTypeFor(t.text);
        if (!ty) {
            error(t.loc, "unknown type '" + t.text + "'");
            ty = tyI64();
        }
        while (is(P::Star)) {
            take();
            ty = tyPtr(ty);
        }
        return ty;
    }

    // Cast lookahead: '(' TYPENAME '*'* ')'
    bool looksLikeCast() {
        if (!is(P::LParen)) return false;
        if (!isTypeName(tok(1))) return false;
        size_t k = 2;
        while (is(P::Star, k)) k++;
        return is(P::RParen, k);
    }

    // ------------------------------------------------------------ const eval (array dims)
    int64_t evalConstI64(const Expr* e, bool* ok) {
        if (!e) {
            *ok = false;
            return 0;
        }
        switch (e->kind) {
            case Ex::IntLit:
            case Ex::CharLit:
                return e->intVal;
            case Ex::FloatLit:
                return (int64_t)e->fltVal;
            case Ex::SizeofType:
                return e->castType ? e->castType->size() : (*ok = false, 0);
            case Ex::Unary: {
                int64_t v = evalConstI64(e->kids[0].get(), ok);
                switch (e->punct) {
                    case P::Minus:
                        return -v;
                    case P::Plus:
                        return v;
                    case P::Tilde:
                        return ~v;
                    case P::Not:
                        return !v;
                    default:
                        *ok = false;
                        return 0;
                }
            }
            case Ex::Binary: {
                int64_t a = evalConstI64(e->kids[0].get(), ok);
                int64_t b = evalConstI64(e->kids[1].get(), ok);
                if (!*ok) return 0;
                switch (e->punct) {
                    case P::Plus:
                        return a + b;
                    case P::Minus:
                        return a - b;
                    case P::Star:
                        return a * b;
                    case P::Slash:
                        return b ? a / b : (*ok = false, 0);
                    case P::Percent:
                        return b ? a % b : (*ok = false, 0);
                    case P::Shl:
                        return (int64_t)((uint64_t)a << (b & 63));
                    case P::Shr:
                        return a >> (b & 63);
                    case P::Amp:
                        return a & b;
                    case P::Pipe:
                        return a | b;
                    case P::Caret:
                        return a ^ b;
                    case P::Pow: {
                        int64_t r = 1;
                        while (b-- > 0) r *= a;
                        return r;
                    }
                    default:
                        *ok = false;
                        return 0;
                }
            }
            default:
                *ok = false;
                return 0;
        }
    }

    // ------------------------------------------------------------ expressions
    ExprPtr mkE(Ex k, const SrcLoc& l) { return std::make_unique<Expr>(k, l); }

    ExprPtr parseExpr() { return parseAssign(); }

    ExprPtr parseAssign() {
        ExprPtr lhs = parseLor();
        P p = tok().kind == Tok::Punct ? tok().punct : P::None;
        switch (p) {
            case P::Assign:
            case P::PlusEq:
            case P::MinusEq:
            case P::StarEq:
            case P::SlashEq:
            case P::PercentEq:
            case P::ShlEq:
            case P::ShrEq:
            case P::AmpEq:
            case P::PipeEq:
            case P::CaretEq: {
                Token op = take();
                ExprPtr rhs = parseAssign();  // right assoc
                ExprPtr e = mkE(Ex::Assign, op.loc);
                e->punct = p;
                e->kids.push_back(std::move(lhs));
                e->kids.push_back(std::move(rhs));
                return e;
            }
            default:
                return lhs;
        }
    }

    ExprPtr binaryLoop(std::function<ExprPtr()> sub, std::initializer_list<P> ops) {
        ExprPtr lhs = sub();
        for (;;) {
            bool matched = false;
            for (P p : ops) {
                if (is(p)) {
                    Token op = take();
                    ExprPtr rhs = sub();
                    ExprPtr e = mkE(Ex::Binary, op.loc);
                    e->punct = p;
                    e->kids.push_back(std::move(lhs));
                    e->kids.push_back(std::move(rhs));
                    lhs = std::move(e);
                    matched = true;
                    break;
                }
            }
            if (!matched) return lhs;
        }
    }

    // TempleOS operator precedence, highest first:
    //   `  >>  <<
    //   *  /  %
    //   &
    //   ^
    //   |
    //   +  -
    //   <  >  <=  >=       (chained)
    //   ==  !=
    //   &&
    //   ^^
    //   ||
    //   assignments
    ExprPtr parseLor() {
        return binaryLoop([this] { return parseLxor(); }, {P::OrOr});
    }
    ExprPtr parseLxor() {
        return binaryLoop([this] { return parseLand(); }, {P::XorXor});
    }
    ExprPtr parseLand() {
        return binaryLoop([this] { return parseEq(); }, {P::AndAnd});
    }
    ExprPtr parseEq() {
        return binaryLoop([this] { return parseRel(); }, {P::EqEq, P::Ne});
    }
    ExprPtr parseRel() {
        // chained: 5<i<j+1<20  ==  5<i && i<j+1 && j+1<20
        ExprPtr first = parseAdd();
        if (!(is(P::Lt) || is(P::Gt) || is(P::Le) || is(P::Ge))) return first;
        ExprPtr e = mkE(Ex::ChainCmp, tok().loc);
        e->kids.push_back(std::move(first));
        while (is(P::Lt) || is(P::Gt) || is(P::Le) || is(P::Ge)) {
            e->chainOps.push_back(take().punct);
            e->kids.push_back(parseAdd());
        }
        return e;
    }
    ExprPtr parseAdd() {
        return binaryLoop([this] { return parseBor(); }, {P::Plus, P::Minus});
    }
    ExprPtr parseBor() {
        return binaryLoop([this] { return parseBxor(); }, {P::Pipe});
    }
    ExprPtr parseBxor() {
        return binaryLoop([this] { return parseBand(); }, {P::Caret});
    }
    ExprPtr parseBand() {
        return binaryLoop([this] { return parseMul(); }, {P::Amp});
    }
    ExprPtr parseMul() {
        return binaryLoop([this] { return parsePow(); }, {P::Star, P::Slash, P::Percent});
    }
    ExprPtr parsePow() {
        return binaryLoop([this] { return parseUnary(); }, {P::Pow, P::Shl, P::Shr});
    }

    ExprPtr parseUnary() {
        const Token& t = tok();
        if (t.kind == Tok::Punct) {
            switch (t.punct) {
                case P::Not:
                case P::Tilde:
                case P::Minus:
                case P::Plus:
                case P::Star:
                case P::Amp: {
                    Token op = take();
                    ExprPtr e = mkE(Ex::Unary, op.loc);
                    e->punct = op.punct;
                    e->prefix = true;
                    e->kids.push_back(parseUnary());
                    return e;
                }
                case P::PlusPlus:
                case P::MinusMinus: {
                    Token op = take();
                    ExprPtr e = mkE(Ex::Unary, op.loc);
                    e->punct = op.punct;
                    e->prefix = true;
                    e->kids.push_back(parseUnary());
                    return e;
                }
                default:
                    break;
            }
        }
        return parsePostfix();
    }

    ExprPtr parsePostfix() {
        ExprPtr e = parsePrimary();
        for (;;) {
            if (looksLikeCast()) {
                // postfix typecast: i(U8*)
                Token lp = take();  // (
                TypePtr ty = parseBaseType();
                expect(P::RParen, "after cast type");
                ExprPtr c = mkE(Ex::Cast, lp.loc);
                c->castType = ty;
                c->kids.push_back(std::move(e));
                e = std::move(c);
                continue;
            }
            if (is(P::LParen)) {
                Token lp = take();
                ExprPtr call = mkE(Ex::Call, lp.loc);
                call->kids.push_back(std::move(e));
                parseCallArgs(*call);
                e = std::move(call);
                continue;
            }
            if (is(P::LBracket)) {
                Token lb = take();
                ExprPtr ix = mkE(Ex::Index, lb.loc);
                ix->kids.push_back(std::move(e));
                ix->kids.push_back(parseExpr());
                expect(P::RBracket, "after index");
                e = std::move(ix);
                continue;
            }
            if (is(P::Dot) || is(P::Arrow)) {
                Token op = take();
                if (tok().kind != Tok::Ident) {
                    error(tok().loc, "expected member name");
                    return e;
                }
                Token name = take();
                ExprPtr m = mkE(Ex::Member, op.loc);
                m->strVal = name.text;
                m->isArrow = op.punct == P::Arrow;
                m->kids.push_back(std::move(e));
                e = std::move(m);
                continue;
            }
            if (is(P::PlusPlus) || is(P::MinusMinus)) {
                Token op = take();
                ExprPtr u = mkE(Ex::Unary, op.loc);
                u->punct = op.punct;
                u->prefix = false;
                u->kids.push_back(std::move(e));
                e = std::move(u);
                continue;
            }
            return e;
        }
    }

    // args with HolyC skipped-arg support: Test(,3)
    void parseCallArgs(Expr& call) {
        if (accept(P::RParen)) return;
        for (;;) {
            if (is(P::Comma) || is(P::RParen)) {
                call.kids.push_back(nullptr);  // missing -> default
                call.argPresent.push_back(false);
            } else {
                call.kids.push_back(parseAssign());
                call.argPresent.push_back(true);
            }
            if (accept(P::Comma)) continue;
            expect(P::RParen, "after call arguments");
            return;
        }
    }

    ExprPtr parsePrimary() {
        Token t = tok();
        switch (t.kind) {
            case Tok::IntLit: {
                take();
                ExprPtr e = mkE(Ex::IntLit, t.loc);
                e->intVal = t.ival;
                return e;
            }
            case Tok::FloatLit: {
                take();
                ExprPtr e = mkE(Ex::FloatLit, t.loc);
                e->fltVal = t.fval;
                return e;
            }
            case Tok::CharLit: {
                take();
                ExprPtr e = mkE(Ex::CharLit, t.loc);
                e->intVal = t.ival;
                e->charLen = (int)t.text.size();
                return e;
            }
            case Tok::StrLit: {
                take();
                ExprPtr e = mkE(Ex::StrLit, t.loc);
                e->strVal = t.text;
                while (tok().kind == Tok::StrLit) e->strVal += take().text;  // adjacency
                return e;
            }
            case Tok::Ident: {
                if (t.text == "sizeof") return parseSizeof();
                if (t.text == "offset") return parseOffset();
                if (t.text == "lastclass") {
                    take();
                    return mkE(Ex::LastClass, t.loc);
                }
                take();
                ExprPtr e = mkE(Ex::Ident, t.loc);
                e->strVal = t.text;
                return e;
            }
            case Tok::Punct:
                if (t.punct == P::LParen) {
                    take();
                    ExprPtr e = parseExpr();
                    expect(P::RParen, "after parenthesized expression");
                    return e;
                }
                if (t.punct == P::LBrace) return parseInitList();
                if (t.punct == P::Dollar) {
                    take();
                    error(t.loc, "'$' (instruction address) is not supported by hcc");
                    return mkE(Ex::IntLit, t.loc);
                }
                break;
            default:
                break;
        }
        error(t.loc, "expected expression (got '" + describe(t) + "')");
        if (t.kind != Tok::Eof) take();
        return mkE(Ex::IntLit, t.loc);
    }

    ExprPtr parseInitList() {
        Token lb = take();  // {
        ExprPtr e = mkE(Ex::InitList, lb.loc);
        if (accept(P::RBrace)) return e;
        for (;;) {
            e->kids.push_back(parseAssign());
            if (accept(P::Comma)) {
                if (is(P::RBrace)) break;  // trailing comma
                continue;
            }
            break;
        }
        expect(P::RBrace, "after initializer list");
        return e;
    }

    ExprPtr parseSizeof() {
        Token kw = take();
        expect(P::LParen, "after sizeof");
        ExprPtr e;
        if (isTypeName(tok()) && !prog->classes.count(tok().text)) {
            // builtin type name -> type sizeof
            e = mkE(Ex::SizeofType, kw.loc);
            e->castType = parseBaseType();
        } else if (prog->classes.count(tok().text)) {
            // class, or class.member ("one level of member vars")
            if (is(P::Dot, 1)) {
                e = mkE(Ex::SizeofType, kw.loc);
                std::string cname = take().text;
                take();  // .
                if (tok().kind != Tok::Ident) {
                    error(tok().loc, "expected member name in sizeof");
                    e->castType = tyI64();
                } else {
                    std::string mname = take().text;
                    auto ci = prog->classes.find(cname);
                    const ClassMember* m = ci->second->findMember(mname);
                    if (!m) {
                        error(kw.loc, "no member '" + mname + "' in class '" + cname + "'");
                        e->castType = tyI64();
                    } else {
                        e->castType = m->type;
                    }
                }
            } else {
                e = mkE(Ex::SizeofType, kw.loc);
                e->castType = parseBaseType();
            }
        } else {
            e = mkE(Ex::SizeofExpr, kw.loc);
            e->kids.push_back(parseExpr());
        }
        expect(P::RParen, "after sizeof");
        return e;
    }

    ExprPtr parseOffset() {
        Token kw = take();
        expect(P::LParen, "after offset");
        ExprPtr e = mkE(Ex::OffsetOf, kw.loc);
        if (tok().kind != Tok::Ident) {
            error(tok().loc, "offset(classname.membername) expected");
        } else {
            e->className = take().text;
            expect(P::Dot, "in offset(classname.membername)");
            if (tok().kind == Tok::Ident)
                e->memberName = take().text;
            else
                error(tok().loc, "expected member name in offset()");
        }
        expect(P::RParen, "after offset()");
        return e;
    }

    // ------------------------------------------------------------ statements
    StmtPtr mkS(St k, const SrcLoc& l) { return std::make_unique<Stmt>(k, l); }

    std::vector<ExprPtr> parseExprList() {
        std::vector<ExprPtr> v;
        v.push_back(parseExpr());
        while (accept(P::Comma)) v.push_back(parseExpr());
        return v;
    }

    StmtPtr parseStmt() {
        const Token& t = tok();

        if (t.is(P::Semi)) {
            Token s = take();
            return mkS(St::Empty, s.loc);
        }
        if (t.is(P::LBrace)) return parseBlock();

        if (t.kind == Tok::StrLit) return parsePrintStmt();
        if (t.kind == Tok::CharLit) return parsePutCharsStmt();

        if (t.kind == Tok::Ident) {
            const std::string& kw = t.text;
            if (kw == "if") return parseIf();
            if (kw == "while") return parseWhile();
            if (kw == "do") return parseDoWhile();
            if (kw == "for") return parseFor();
            if (kw == "switch") return parseSwitch();
            if (kw == "break") {
                Token b = take();
                expect(P::Semi, "after break");
                return mkS(St::Break, b.loc);
            }
            if (kw == "continue") {
                error(t.loc, "there is no 'continue' stmt in HolyC -- use goto");
                take();
                accept(P::Semi);
                return mkS(St::Empty, t.loc);
            }
            if (kw == "goto") {
                Token g = take();
                StmtPtr s = mkS(St::Goto, g.loc);
                if (tok().kind == Tok::Ident)
                    s->label = take().text;
                else
                    error(tok().loc, "expected label after goto");
                expect(P::Semi, "after goto");
                return s;
            }
            if (kw == "return") {
                Token r = take();
                StmtPtr s = mkS(St::Return, r.loc);
                if (!is(P::Semi)) s->expr = parseExpr();
                expect(P::Semi, "after return");
                return s;
            }
            if (kw == "case") return parseCase();
            if (kw == "default" && is(P::Colon, 1)) {
                Token d = take();
                take();  // :
                return mkS(St::Default, d.loc);
            }
            if (kw == "start" && is(P::Colon, 1)) {
                Token d = take();
                take();
                return mkS(St::SubStart, d.loc);
            }
            if (kw == "end" && is(P::Colon, 1)) {
                Token d = take();
                take();
                return mkS(St::SubEnd, d.loc);
            }
            if (kw == "try") return parseTry();
            if (kw == "no_warn") {
                Token n = take();
                StmtPtr s = mkS(St::NoWarn, n.loc);
                if (!is(P::Semi)) parseExprList();  // parse & discard
                expect(P::Semi, "after no_warn");
                return s;
            }
            if (kw == "lock") {
                Token l = take();
                StmtPtr s = mkS(St::Lock, l.loc);
                s->body.push_back(parseStmt());
                return s;
            }
            if (kw == "asm") {
                error(t.loc, "inline asm blocks are not supported by hcc");
                take();
                skipBraceBlock();
                return mkS(St::Empty, t.loc);
            }
            if (kw == "class" || kw == "union") {
                error(t.loc, "class definitions are only allowed at top level");
                take();
                skipToSemi();
                return mkS(St::Empty, t.loc);
            }
            // label?
            if (is(P::Colon, 1) && !isTypeName(t)) {
                Token name = take();
                take();  // :
                StmtPtr s = mkS(St::Label, name.loc);
                s->label = name.text;
                return s;
            }
            // declaration?
            if (isTypeName(t) && startsDecl()) return parseVarDeclStmt(false);
            if (kw == "static") {
                take();
                return parseVarDeclStmt(true);
            }
        }

        // expression statement
        StmtPtr s = mkS(St::Expr, t.loc);
        s->expr = parseExpr();
        expect(P::Semi, "after expression");
        return s;
    }

    // A type name starts a declaration unless it is used as an expression
    // (e.g. postfix cast of a paren expr can't start a stmt in practice).
    bool startsDecl() {
        // TYPE ('*'* ident) or TYPE '(' '*' ident ')' (function ptr)
        size_t k = 1;
        while (is(P::Star, k)) k++;
        if (tok(k).kind == Tok::Ident) return true;
        if (is(P::LParen, k) && is(P::Star, k + 1)) return true;
        return false;
    }

    void skipToSemi() {
        int depth = 0;
        for (;;) {
            const Token& t = tok();
            if (t.kind == Tok::Eof) return;
            if (t.is(P::LBrace)) depth++;
            if (t.is(P::RBrace)) depth--;
            if (t.is(P::Semi) && depth <= 0) {
                take();
                return;
            }
            take();
        }
    }
    void skipBraceBlock() {
        if (!is(P::LBrace)) {
            skipToSemi();
            return;
        }
        int depth = 0;
        for (;;) {
            const Token& t = tok();
            if (t.kind == Tok::Eof) return;
            if (t.is(P::LBrace)) depth++;
            if (t.is(P::RBrace)) {
                depth--;
                take();
                if (depth == 0) return;
                continue;
            }
            take();
        }
    }

    StmtPtr parseBlock() {
        Token lb = take();  // {
        StmtPtr s = mkS(St::Block, lb.loc);
        while (!is(P::RBrace)) {
            if (tok().kind == Tok::Eof) {
                error(lb.loc, "unterminated block");
                return s;
            }
            s->body.push_back(parseStmt());
        }
        take();  // }
        return s;
    }

    // "Hello %d\n",x;   |   "" fmt,args;  -> Print(...)
    StmtPtr parsePrintStmt() {
        Token first = tok();
        std::string fmt;
        bool sawPiece = false;
        while (tok().kind == Tok::StrLit) {
            fmt += take().text;
            sawPiece = true;
        }
        StmtPtr s = mkS(St::Expr, first.loc);
        ExprPtr call = mkE(Ex::Call, first.loc);
        ExprPtr callee = mkE(Ex::Ident, first.loc);
        callee->strVal = "Print";
        call->kids.push_back(std::move(callee));

        if (!is(P::Comma) && !is(P::Semi) && fmt.empty() && sawPiece) {
            // "" fmt_expr[,args]  -- empty string signals variable fmt_str
            call->kids.push_back(parseAssign());
            call->argPresent.push_back(true);
        } else {
            ExprPtr f = mkE(Ex::StrLit, first.loc);
            f->strVal = fmt;
            call->kids.push_back(std::move(f));
            call->argPresent.push_back(true);
        }
        while (accept(P::Comma)) {
            call->kids.push_back(parseAssign());
            call->argPresent.push_back(true);
        }
        expect(P::Semi, "after print statement");
        s->expr = std::move(call);
        return s;
    }

    // 'c';   |   '' expr;   -> PutChars(...)
    StmtPtr parsePutCharsStmt() {
        Token lit = take();
        StmtPtr s = mkS(St::Expr, lit.loc);
        ExprPtr call = mkE(Ex::Call, lit.loc);
        ExprPtr callee = mkE(Ex::Ident, lit.loc);
        callee->strVal = "PutChars";
        call->kids.push_back(std::move(callee));
        if (lit.text.empty()) {
            call->kids.push_back(parseAssign());  // '' drv;
        } else {
            ExprPtr c = mkE(Ex::CharLit, lit.loc);
            c->intVal = lit.ival;
            c->charLen = (int)lit.text.size();
            call->kids.push_back(std::move(c));
        }
        call->argPresent.push_back(true);
        expect(P::Semi, "after char statement");
        s->expr = std::move(call);
        return s;
    }

    StmtPtr parseIf() {
        Token kw = take();
        StmtPtr s = mkS(St::If, kw.loc);
        expect(P::LParen, "after if");
        s->expr = parseExpr();
        expect(P::RParen, "after if condition");
        s->body.push_back(parseStmt());
        if (isIdent("else")) {
            take();
            s->body2.push_back(parseStmt());
        }
        return s;
    }

    StmtPtr parseWhile() {
        Token kw = take();
        StmtPtr s = mkS(St::While, kw.loc);
        expect(P::LParen, "after while");
        s->expr = parseExpr();
        expect(P::RParen, "after while condition");
        s->body.push_back(parseStmt());
        return s;
    }

    StmtPtr parseDoWhile() {
        Token kw = take();
        StmtPtr s = mkS(St::DoWhile, kw.loc);
        s->body.push_back(parseStmt());
        if (!isIdent("while")) {
            error(tok().loc, "expected while after do body");
            return s;
        }
        take();
        expect(P::LParen, "after do..while");
        s->expr = parseExpr();
        expect(P::RParen, "after do..while condition");
        expect(P::Semi, "after do..while");
        return s;
    }

    StmtPtr parseFor() {
        Token kw = take();
        StmtPtr s = mkS(St::For, kw.loc);
        expect(P::LParen, "after for");
        if (!is(P::Semi)) s->exprs = parseExprList();  // init
        expect(P::Semi, "after for-init");
        if (!is(P::Semi)) s->expr2 = parseExpr();  // cond
        expect(P::Semi, "after for-condition");
        if (!is(P::RParen)) {
            auto post = parseExprList();
            for (auto& e : post) {
                s->body2.push_back(mkS(St::Expr, e->loc));
                s->body2.back()->expr = std::move(e);
            }
        }
        expect(P::RParen, "after for header");
        s->body.push_back(parseStmt());
        return s;
    }

    StmtPtr parseSwitch() {
        Token kw = take();
        StmtPtr s = mkS(St::Switch, kw.loc);
        if (accept(P::LBracket)) {
            // switch [expr] -- unchecked jump table
            s->switchUnchecked = true;
            s->expr = parseExpr();
            expect(P::RBracket, "after switch [expr]");
        } else {
            expect(P::LParen, "after switch");
            s->expr = parseExpr();
            expect(P::RParen, "after switch expression");
        }
        s->body.push_back(parseStmt());
        return s;
    }

    StmtPtr parseCase() {
        Token kw = take();
        StmtPtr s = mkS(St::Case, kw.loc);
        if (!is(P::Colon)) {
            s->hasCaseVal = true;
            s->caseLo = parseExpr();
            if (accept(P::Ellipsis)) {
                s->hasHi = true;
                s->caseHi = parseExpr();
            }
        }
        expect(P::Colon, "after case");
        return s;
    }

    StmtPtr parseTry() {
        Token kw = take();
        StmtPtr s = mkS(St::Try, kw.loc);
        if (!is(P::LBrace)) {
            error(tok().loc, "expected '{' after try");
            return s;
        }
        StmtPtr b = parseBlock();
        s->body = std::move(b->body);
        if (!isIdent("catch")) {
            error(tok().loc, "expected catch after try block");
            return s;
        }
        take();
        if (!is(P::LBrace)) {
            error(tok().loc, "expected '{' after catch");
            return s;
        }
        StmtPtr c = parseBlock();
        s->body2 = std::move(c->body);
        return s;
    }

    // declarator := ('*' | 'reg' [REGNAME] | 'noreg')* NAME ('[' dim ']')* ['=' init]
    //             | '*'* '(' '*' NAME ')' '(' params ')'
    // returns false if no declarator present
    bool parseDeclarator(TypePtr base, VarDeclarator& d, bool allowInit, bool isParam) {
        TypePtr ty = base;
        for (;;) {
            if (is(P::Star)) {
                take();
                ty = tyPtr(ty);
                continue;
            }
            if (isIdent("reg")) {
                take();
                d.isReg = true;
                // optional register name: I64 reg R15 i=5
                if (tok().kind == Tok::Ident && tok(1).kind == Tok::Ident) d.regName = take().text;
                continue;
            }
            if (isIdent("noreg")) {
                take();
                d.noreg = true;
                continue;
            }
            break;
        }
        // function pointer declarator
        if (is(P::LParen) && is(P::Star, 1)) {
            take();
            take();
            if (tok().kind != Tok::Ident) {
                error(tok().loc, "expected function pointer name");
                return false;
            }
            Token name = take();
            d.name = name.text;
            d.loc = name.loc;
            expect(P::RParen, "after function pointer name");
            Type ft(Type::Func);
            ft.ret = ty;
            ft.params = std::make_shared<std::vector<FuncParam>>();
            expect(P::LParen, "in function pointer declarator");
            parseParams(*ft.params, ft.variadic);
            d.type = tyPtr(std::make_shared<Type>(ft));
        } else {
            if (tok().kind != Tok::Ident) {
                if (isParam) {  // unnamed parameter: U0 F(I64)
                    d.name = "";
                    d.type = ty;
                    return true;
                }
                return false;
            }
            Token name = take();
            d.name = name.text;
            d.loc = name.loc;
            // array dims (innermost last)
            std::vector<int64_t> dims;
            while (accept(P::LBracket)) {
                if (is(P::RBracket) && isParam) {
                    // I64 argv[] -- parameter arrays decay to pointers
                    take();
                    ty = tyPtr(ty);
                    continue;
                }
                ExprPtr dim = parseExpr();
                bool ok = true;
                int64_t n = evalConstI64(dim.get(), &ok);
                if (!ok || n < 0) {
                    error(name.loc, "array dimension must be a constant expression");
                    n = 1;
                }
                dims.push_back(n);
                expect(P::RBracket, "after array dimension");
            }
            for (size_t i = dims.size(); i-- > 0;) ty = tyArray(ty, dims[i]);
            d.type = ty;
        }
        if (allowInit && accept(P::Assign)) d.init = parseAssign();
        return true;
    }

    void parseParams(std::vector<FuncParam>& params, bool& variadic) {
        variadic = false;
        if (accept(P::RParen)) return;
        for (;;) {
            if (accept(P::Ellipsis)) {
                variadic = true;
                expect(P::RParen, "after ...");
                return;
            }
            if (!isTypeName(tok())) {
                error(tok().loc, "expected parameter type (got '" + describe(tok()) + "')");
                skipToParenEnd();
                return;
            }
            TypePtr base = parseBaseType();
            FuncParam p;
            p.loc = tok().loc;
            VarDeclarator d;
            if (tok().kind == Tok::Ident || is(P::LParen)) {
                parseDeclarator(base, d, false, true);
                p.name = d.name;
                p.type = d.type ? d.type : base;
            } else {
                p.type = base;  // unnamed
            }
            if (accept(P::Assign)) {
                if (isIdent("lastclass")) {
                    take();
                    p.dfltIsLastClass = true;
                } else {
                    p.dflt = std::shared_ptr<Expr>(parseAssign().release());
                }
            }
            params.push_back(std::move(p));
            if (accept(P::Comma)) continue;
            expect(P::RParen, "after parameters");
            return;
        }
    }

    void skipToParenEnd() {
        int depth = 1;
        for (;;) {
            const Token& t = tok();
            if (t.kind == Tok::Eof) return;
            if (t.is(P::LParen)) depth++;
            if (t.is(P::RParen)) {
                if (--depth == 0) {
                    take();
                    return;
                }
            }
            take();
        }
    }

    StmtPtr parseVarDeclStmt(bool isStatic) {
        Token first = tok();
        TypePtr base = parseBaseTypeNoStars();
        StmtPtr s = mkS(St::VarDecl, first.loc);
        s->isStatic = isStatic;
        for (;;) {
            VarDeclarator d;
            if (!parseDeclarator(base, d, true, false)) {
                error(tok().loc, "expected variable name");
                skipToSemi();
                return s;
            }
            s->decls.push_back(std::move(d));
            if (accept(P::Comma)) continue;
            expect(P::Semi, "after variable declaration");
            return s;
        }
    }

    // base type WITHOUT consuming '*' (stars belong to each declarator:
    // "U8 *a,b" declares U8* a and U8 b)
    TypePtr parseBaseTypeNoStars() {
        Token t = take();
        TypePtr ty = baseTypeFor(t.text);
        if (!ty) {
            error(t.loc, "unknown type '" + t.text + "'");
            ty = tyI64();
        }
        return ty;
    }

    // ------------------------------------------------------------ top level

    struct Quals {
        Linkage linkage = Linkage::Normal;
        std::string aliasSym;
        bool interrupt = false, hasErrCode = false, argPop = false, noArgPop = false;
        bool any = false;
    };

    Quals parseQuals() {
        Quals q;
        for (;;) {
            const Token& t = tok();
            if (!isQualKw(t)) return q;
            q.any = true;
            std::string kw = take().text;
            if (kw == "public")
                q.linkage = Linkage::Public;
            else if (kw == "static")
                q.linkage = Linkage::Static;
            else if (kw == "extern")
                q.linkage = Linkage::Extern;
            else if (kw == "import")
                q.linkage = Linkage::Import;
            else if (kw == "_extern" || kw == "_import") {
                q.linkage = kw == "_extern" ? Linkage::ExternAlias : Linkage::ImportAlias;
                if (tok().kind == Tok::Ident)
                    q.aliasSym = take().text;
                else
                    error(tok().loc, "expected symbol name after " + kw);
            } else if (kw == "interrupt")
                q.interrupt = true;
            else if (kw == "haserrcode")
                q.hasErrCode = true;
            else if (kw == "argpop")
                q.argPop = true;
            else if (kw == "noargpop")
                q.noArgPop = true;
        }
    }

    void parseClassDef(const Quals& q) {
        // [WHOLETYPE] (class|union) NAME [:BASE] [{...} decls] ';'
        TypePtr wholeType;
        if (!isIdent("class") && !isIdent("union")) {
            // typed union: "I64i union I64 { ... }"
            wholeType = parseBaseType();
        }
        bool isUnion = isIdent("union");
        Token kw = take();  // class|union
        if (tok().kind != Tok::Ident) {
            error(tok().loc, "expected class name");
            skipToSemi();
            return;
        }
        Token name = take();

        std::shared_ptr<ClassInfo> ci;
        auto existing = prog->classes.find(name.text);
        if (existing != prog->classes.end()) {
            ci = existing->second;
            if (ci->complete && (is(P::LBrace) || is(P::Colon)))
                error(name.loc, "class '" + name.text + "' redefined");
        } else {
            ci = std::make_shared<ClassInfo>();
            ci->name = name.text;
            prog->classes[name.text] = ci;
        }
        ci->isUnion = isUnion;
        ci->wholeType = wholeType;

        if (accept(P::Colon)) {  // single inheritance
            if (tok().kind != Tok::Ident || !prog->classes.count(tok().text)) {
                error(tok().loc, "expected base class name");
            } else {
                Token base = take();
                ci->base = baseTypeFor(base.text);
                if (is(P::Comma)) error(tok().loc, "only one base class is allowed");
            }
        }

        if (accept(P::Semi)) return;  // forward declaration

        expect(P::LBrace, "in class definition");
        int64_t offset = 0;
        if (ci->base && ci->base->cls) {
            offset = ci->base->cls->size;
            ci->align = ci->base->cls->align;
        }
        int64_t maxSize = offset;
        while (!is(P::RBrace)) {
            if (tok().kind == Tok::Eof) {
                error(kw.loc, "unterminated class definition");
                return;
            }
            if (accept(P::Semi)) continue;
            if (is(P::Dollar)) {
                error(tok().loc, "'$' offset hacks in classes are not supported by hcc");
                skipToSemi();
                continue;
            }
            if (!isTypeName(tok())) {
                error(tok().loc, "expected member type (got '" + describe(tok()) + "')");
                skipToSemi();
                continue;
            }
            TypePtr base = parseBaseTypeNoStars();
            for (;;) {
                VarDeclarator d;
                if (!parseDeclarator(base, d, false, false)) {
                    error(tok().loc, "expected member name");
                    break;
                }
                ClassMember m;
                m.name = d.name;
                m.type = d.type;
                // member meta data: IDENT literal-expr, repeated
                while (tok().kind == Tok::Ident && !is(P::Comma) && !is(P::Semi)) {
                    std::string metaName = take().text;
                    Token metaVal = tok();
                    parseAssign();  // value expr (stored raw; reflection not supported)
                    m.meta.emplace_back(metaName, metaVal);
                }
                int64_t al = m.type->align() ? m.type->align() : 1;
                int64_t sz = m.type->size();
                if (isUnion) {
                    m.offset = 0;
                    if (sz > maxSize) maxSize = sz;
                } else {
                    offset = (offset + al - 1) / al * al;
                    m.offset = offset;
                    offset += sz;
                    if (offset > maxSize) maxSize = offset;
                }
                if (al > ci->align) ci->align = al;
                // multiple "pad"/"reserved" members allowed by spec
                ci->members.push_back(std::move(m));
                if (accept(P::Comma)) continue;
                expect(P::Semi, "after class member");
                break;
            }
        }
        take();  // }
        ci->size = (maxSize + ci->align - 1) / ci->align * ci->align;
        if (ci->size == 0) ci->size = 0;  // zero-size classes allowed (U0-spirit)
        ci->complete = true;

        TopItem item;
        item.classDef = ci;
        prog->items.push_back(std::move(item));

        // instance declarators after '}': class Foo {...} a,b;
        if (!is(P::Semi)) {
            for (;;) {
                VarDeclarator d;
                if (!parseDeclarator(baseTypeFor(ci->name), d, true, false)) {
                    error(tok().loc, "expected declarator after class definition");
                    break;
                }
                addGlobalOrLocalTop(std::move(d), q);
                if (accept(P::Comma)) continue;
                break;
            }
        }
        expect(P::Semi, "after class definition");
    }

    void addGlobalOrLocalTop(VarDeclarator d, const Quals& q) {
        auto g = std::make_unique<GlobalVar>();
        g->name = d.name;
        g->type = d.type;
        g->init = std::move(d.init);
        g->linkage = q.linkage;
        g->aliasSym = q.aliasSym;
        g->loc = d.loc;
        TopItem item;
        item.global = std::move(g);
        prog->items.push_back(std::move(item));
    }

    void parseTopLevel() {
        if (accept(P::Semi)) return;

        Quals q = parseQuals();

        // class/union (possibly with whole-type prefix)
        if (isIdent("class") || isIdent("union") ||
            (isTypeName(tok()) && (isIdent("class", 1) || isIdent("union", 1)))) {
            parseClassDef(q);
            return;
        }

        if (isTypeName(tok()) && (startsDecl() || q.any)) {
            // could be function or global var(s)
            Token first = tok();
            TypePtr base = parseBaseTypeNoStars();
            // count stars to find name
            TypePtr ty = base;
            while (is(P::Star)) {
                take();
                ty = tyPtr(ty);
            }
            if (tok().kind == Tok::Ident && is(P::LParen, 1)) {
                parseFunc(ty, q);
                return;
            }
            // global variable list; first declarator's stars already consumed
            for (bool firstDecl = true;; firstDecl = false) {
                VarDeclarator d;
                TypePtr dbase = firstDecl ? ty : base;
                if (!parseDeclarator(dbase, d, true, false)) {
                    error(tok().loc, "expected declarator");
                    skipToSemi();
                    return;
                }
                addGlobalOrLocalTop(std::move(d), q);
                if (accept(P::Comma)) continue;
                expect(P::Semi, "after global variable");
                return;
            }
        }

        if (q.any) {
            error(tok().loc, "expected declaration after qualifiers");
            skipToSemi();
            return;
        }

        // top-level statement: "code outside of functions gets executed
        // upon start-up, in order"
        TopItem item;
        item.stmt = parseStmt();
        prog->items.push_back(std::move(item));
    }

    void parseFunc(TypePtr retTy, const Quals& q) {
        Token name = take();
        take();  // (
        auto fd = std::make_unique<FuncDecl>();
        fd->name = name.text;
        fd->loc = name.loc;
        fd->linkage = q.linkage;
        fd->aliasSym = q.aliasSym;
        fd->interrupt = q.interrupt;
        fd->hasErrCode = q.hasErrCode;
        fd->argPop = q.argPop;
        fd->noArgPop = q.noArgPop;

        Type ft(Type::Func);
        ft.ret = retTy;
        ft.params = std::make_shared<std::vector<FuncParam>>();
        parseParams(*ft.params, ft.variadic);
        fd->type = std::make_shared<Type>(ft);

        if (is(P::LBrace)) {
            StmtPtr body = parseBlock();
            fd->body = std::move(body->body);
            fd->hasBody = true;
        } else {
            expect(P::Semi, "after function prototype");
        }
        TopItem item;
        item.func = std::move(fd);
        prog->items.push_back(std::move(item));
    }

    std::unique_ptr<Program> run() {
        while (tok().kind != Tok::Eof) parseTopLevel();
        return std::move(prog);
    }
};

Parser::Parser(Lexer& lx) { impl_ = std::make_shared<Impl>(lx); }

std::unique_ptr<Program> Parser::parseProgram() {
    auto p = impl_->run();
    hadError_ = impl_->hadError || impl_->lx.hadError();
    return p;
}

// ------------------------------------------------------------------ dump

namespace {
struct Dumper {
    std::string out;
    int depth = 0;
    void line(const std::string& s) {
        out.append(depth * 2, ' ');
        out += s;
        out += '\n';
    }
    void expr(const Expr* e) {
        if (!e) {
            line("(missing)");
            return;
        }
        switch (e->kind) {
            case Ex::IntLit:
                line("(int " + std::to_string(e->intVal) + ")");
                return;
            case Ex::FloatLit:
                line("(float " + std::to_string(e->fltVal) + ")");
                return;
            case Ex::StrLit:
                line("(str \"" + e->strVal + "\")");
                return;
            case Ex::CharLit:
                line("(char " + std::to_string(e->intVal) + ")");
                return;
            case Ex::Ident:
                line("(ident " + e->strVal + ")");
                return;
            case Ex::LastClass:
                line("(lastclass)");
                return;
            case Ex::SizeofType:
                line("(sizeof-type " + e->castType->str() + ")");
                return;
            case Ex::OffsetOf:
                line("(offset " + e->className + "." + e->memberName + ")");
                return;
            default:
                break;
        }
        std::string head = "(";
        switch (e->kind) {
            case Ex::Unary:
                head +=
                    std::string("unary") + (e->prefix ? "-pre " : "-post ") + punctStr(e->punct);
                break;
            case Ex::Binary:
                head += std::string("binary ") + punctStr(e->punct);
                break;
            case Ex::Assign:
                head += std::string("assign ") + punctStr(e->punct);
                break;
            case Ex::ChainCmp: {
                head += "chain-cmp";
                for (P p : e->chainOps) head += std::string(" ") + punctStr(p);
                break;
            }
            case Ex::Call:
                head += "call";
                break;
            case Ex::Index:
                head += "index";
                break;
            case Ex::Member:
                head += std::string(e->isArrow ? "arrow " : "member ") + e->strVal;
                break;
            case Ex::Cast:
                head += "cast " + e->castType->str();
                break;
            case Ex::SizeofExpr:
                head += "sizeof-expr";
                break;
            case Ex::InitList:
                head += "init-list";
                break;
            default:
                head += "expr?";
                break;
        }
        line(head);
        depth++;
        for (auto& k : e->kids) expr(k.get());
        depth--;
        line(")");
    }
    void stmt(const Stmt* s) {
        std::string head = "(";
        switch (s->kind) {
            case St::Block:
                head += "block";
                break;
            case St::Expr:
                head += "expr-stmt";
                break;
            case St::If:
                head += "if";
                break;
            case St::While:
                head += "while";
                break;
            case St::DoWhile:
                head += "do-while";
                break;
            case St::For:
                head += "for";
                break;
            case St::Switch:
                head += s->switchUnchecked ? "switch-unchecked" : "switch";
                break;
            case St::Case:
                head += s->hasCaseVal ? "case" : "case-null";
                break;
            case St::Default:
                line("(default)");
                return;
            case St::SubStart:
                line("(sub-start)");
                return;
            case St::SubEnd:
                line("(sub-end)");
                return;
            case St::Break:
                line("(break)");
                return;
            case St::Goto:
                line("(goto " + s->label + ")");
                return;
            case St::Label:
                line("(label " + s->label + ")");
                return;
            case St::Return:
                head += "return";
                break;
            case St::VarDecl:
                head += "var-decl";
                break;
            case St::Try:
                head += "try";
                break;
            case St::NoWarn:
                line("(no_warn)");
                return;
            case St::Lock:
                head += "lock";
                break;
            case St::Empty:
                line("(empty)");
                return;
        }
        line(head);
        depth++;
        if (s->expr) expr(s->expr.get());
        if (s->expr2) expr(s->expr2.get());
        for (auto& e : s->exprs) expr(e.get());
        if (s->caseLo) expr(s->caseLo.get());
        if (s->caseHi) expr(s->caseHi.get());
        for (auto& d : s->decls) {
            line("(decl " + d.name + " : " + (d.type ? d.type->str() : "?") + ")");
            if (d.init) {
                depth++;
                expr(d.init.get());
                depth--;
            }
        }
        for (auto& b : s->body) stmt(b.get());
        if (!s->body2.empty()) {
            line("(else/catch/post");
            depth++;
            for (auto& b : s->body2) stmt(b.get());
            depth--;
            line(")");
        }
        depth--;
        line(")");
    }
};
}  // namespace

std::string dumpProgram(const Program& p) {
    Dumper d;
    for (auto& item : p.items) {
        if (item.classDef) {
            auto& c = *item.classDef;
            d.line("(class " + c.name + (c.isUnion ? " union" : "") +
                   " size=" + std::to_string(c.size));
            d.depth++;
            for (auto& m : c.members)
                d.line("(member " + m.name + " : " + m.type->str() + " @" +
                       std::to_string(m.offset) + ")");
            d.depth--;
            d.line(")");
        } else if (item.func) {
            auto& f = *item.func;
            std::string head = "(func " + f.name + " : " + f.type->ret->str() + " (";
            for (size_t i = 0; i < f.type->params->size(); i++) {
                auto& p2 = (*f.type->params)[i];
                if (i) head += ", ";
                head += p2.type->str() + " " + p2.name;
                if (p2.dflt) head += "=<dflt>";
                if (p2.dfltIsLastClass) head += "=lastclass";
            }
            if (f.type->variadic) head += ", ...";
            head += ")" + std::string(f.hasBody ? "" : " proto");
            d.line(head);
            d.depth++;
            for (auto& s : f.body) d.stmt(s.get());
            d.depth--;
            d.line(")");
        } else if (item.global) {
            auto& g = *item.global;
            d.line("(global " + g.name + " : " + (g.type ? g.type->str() : "?") + ")");
            if (g.init) {
                d.depth++;
                d.expr(g.init.get());
                d.depth--;
            }
        } else if (item.stmt) {
            d.stmt(item.stmt.get());
        }
    }
    return d.out;
}

}  // namespace hc
