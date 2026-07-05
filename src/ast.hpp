// hlang -- HolyC AST and type representation.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lexer.hpp"

namespace hc {

// ------------------------------------------------------------------ types

struct Type;
using TypePtr = std::shared_ptr<Type>;

struct ClassMember {
    std::string name;
    TypePtr type;
    int64_t offset = 0;
    // member meta data (ClassMeta.HC): name -> literal token (int/F64/string/&fun ident)
    std::vector<std::pair<std::string, Token>> meta;
};

struct ClassInfo {
    std::string name;
    bool isUnion = false;
    TypePtr base;       // single inheritance
    TypePtr wholeType;  // "I64i union I64 {...}" -- type used when accessed whole
    std::vector<ClassMember> members;
    int64_t size = 0;  // packed: sum of member sizes (no C-style padding)
    bool complete = false;
    const ClassMember* findMember(const std::string& n) const;
};

struct FuncParam;

struct Type {
    enum Kind {
        Void,  // U0 / I0: ZERO size
        Int,   // size 1/2/4/8, signed or not
        F64,
        Ptr,
        Array,
        Class,
        Func,
    } kind;

    Type() : kind(Void) {}
    explicit Type(Kind k) : kind(k) {}

    // Int
    int intSize = 8;
    bool isUnsigned = false;
    // Ptr/Array
    TypePtr elem;
    int64_t arrayLen = 0;
    // Class
    std::shared_ptr<ClassInfo> cls;
    // Func
    TypePtr ret;
    std::shared_ptr<std::vector<FuncParam>> params;
    bool variadic = false;  // HolyC ... (argc/argv)

    int64_t size() const;
    bool isF64() const { return kind == F64; }
    bool isInt() const { return kind == Int; }
    bool isPtr() const { return kind == Ptr; }
    std::string str() const;
};

TypePtr tyVoid();
TypePtr tyInt(int size, bool uns);
TypePtr tyI64();
TypePtr tyU64();
TypePtr tyU8();
TypePtr tyF64();
TypePtr tyPtr(TypePtr elem);
TypePtr tyArray(TypePtr elem, int64_t n);

// ------------------------------------------------------------------ expressions

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class Ex {
    IntLit,
    FloatLit,
    StrLit,   // text in strVal
    CharLit,  // packed in intVal; empty '' has charLen==0
    Ident,
    Unary,     // op in punct: ! ~ - & * ++ -- (prefix flag)
    Binary,    // op in punct
    ChainCmp,  // a<b<=c : kids operands, ops in chainOps
    Assign,    // op in punct (Assign, PlusEq, ...)
    Call,      // kids[0]=callee, args follow; argPresent marks skipped default slots
    Index,     // kids[0][kids[1]]
    Member,    // kids[0] . strVal   (isArrow for ->)
    Cast,      // postfix: kids[0](castType)
    SizeofType,
    SizeofExpr,
    OffsetOf,     // strVal="Class.member" via className/memberName
    NoParenCall,  // resolved in codegen: Ident that may be an implicit call
    LastClass,    // 'lastclass' default-arg keyword (materialized at call site)
    InitList,     // {1,2,3} brace initializer
};

struct Expr {
    Ex kind;
    SrcLoc loc;
    int64_t intVal = 0;
    double fltVal = 0;
    std::string strVal;   // ident name / string bytes / member name
    int charLen = 0;      // CharLit raw length (0 for '')
    P punct = P::None;    // operator
    bool prefix = false;  // for Unary ++/--
    bool isArrow = false;
    TypePtr castType;                     // Cast/SizeofType
    std::string className, memberName;    // OffsetOf
    std::shared_ptr<ClassInfo> classRef;  // OffsetOf: pinned at parse (class dups overshadow)
    std::vector<ExprPtr> kids;
    std::vector<P> chainOps;       // ChainCmp
    std::vector<bool> argPresent;  // Call: per-arg "was written" (for Test(,3))

    Expr(Ex k, SrcLoc l) : kind(k), loc(std::move(l)) {}
};

// ------------------------------------------------------------------ statements

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

enum class St {
    Block,
    Expr,
    If,
    While,
    DoWhile,
    For,
    Switch,
    Case,  // case values in caseLo/caseHi (null case: hasCaseVal=false); range via hasHi
    Default,
    SubStart,  // sub_switch 'start:'
    SubEnd,    // sub_switch 'end:'
    Break,
    Goto,
    Label,
    Return,
    VarDecl,
    Try,
    NoWarn,
    Lock,
    Empty,
};

struct VarDeclarator {
    std::string name;
    TypePtr type;
    ExprPtr init;  // may be null
    bool noreg = false, isReg = false;
    std::string regName;
    SrcLoc loc;
};

struct Stmt {
    St kind;
    SrcLoc loc;
    // generic children
    std::vector<StmtPtr> body;   // Block items / single bodies
    std::vector<StmtPtr> body2;  // else branch / catch block / for-post
    ExprPtr expr;                // condition / expression / return value / case expr
    ExprPtr expr2;               // for-cond
    std::vector<ExprPtr> exprs;  // for-init/post expression lists
    std::string label;           // goto/label name
    // Case
    ExprPtr caseLo, caseHi;
    bool hasCaseVal = false, hasHi = false;
    // Switch
    bool switchUnchecked = false;  // switch [expr]
    // VarDecl
    std::vector<VarDeclarator> decls;
    bool isStatic = false;

    Stmt(St k, SrcLoc l) : kind(k), loc(std::move(l)) {}
};

// ------------------------------------------------------------------ top level

struct FuncParam {
    std::string name;
    TypePtr type;
    std::shared_ptr<Expr> dflt;  // default arg expr (shared: type may be reused)
    bool dfltIsLastClass = false;
    SrcLoc loc;
};

enum class Linkage { Normal, Public, Extern, Import, ExternAlias, ImportAlias, Static };

struct FuncDecl {
    std::string name;
    std::string aliasSym;  // _extern/_import symbol
    TypePtr type;          // Func type
    Linkage linkage = Linkage::Normal;
    bool interrupt = false, hasErrCode = false, argPop = false, noArgPop = false;
    std::vector<StmtPtr> body;
    bool hasBody = false;
    SrcLoc loc;
};

struct GlobalVar {
    std::string name;
    std::string aliasSym;
    TypePtr type;
    ExprPtr init;
    Linkage linkage = Linkage::Normal;
    SrcLoc loc;
};

struct TopItem {
    // exactly one of these is set
    std::unique_ptr<FuncDecl> func;
    std::unique_ptr<GlobalVar> global;
    StmtPtr stmt;                         // top-level statement (implicit main)
    std::shared_ptr<ClassInfo> classDef;  // class/union definition
};

struct Program {
    std::vector<TopItem> items;
    std::map<std::string, std::shared_ptr<ClassInfo>> classes;
};

}  // namespace hc
