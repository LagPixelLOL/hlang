// hlang -- LLVM IR generation from the HolyC AST.
//
// Value model (TempleOS doctrine: "All values are extended to 64-bit when
// accessed. Intermediate calculations are done with 64-bit values."):
//   - integer and pointer rvalues are i64
//   - F64 rvalues are double
//   - memory uses natural widths (i8/i16/i32/i64/double); aggregates are
//     [N x i8] with manual layout, addressed by byte GEPs
//   - pointer arithmetic (+ - ++ -- += -=) is UNSCALED (pointers are just
//     I64s); indexing p[i] DOES scale by element size
#include "codegen.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>

#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <vector>

using namespace llvm;

namespace hc {
namespace {

struct RV {              // rvalue
    Value* v = nullptr;  // i64 | double | nullptr (void)
    TypePtr t;
};

struct LV {                 // lvalue
    Value* addr = nullptr;  // LLVM ptr
    TypePtr t;
};

struct VarSym {
    Value* addr = nullptr;  // ptr to storage
    TypePtr type;
};

struct FuncSym {
    Function* fn = nullptr;
    TypePtr type;  // Func type (params/defaults)
    const FuncDecl* decl = nullptr;
};

struct SwitchCtx;

struct BreakTarget {
    BasicBlock* bb = nullptr;
    SwitchCtx* swc = nullptr;  // set for switch scopes (sub_switch break)
};

struct SwitchCtx {
    SwitchInst* sw = nullptr;
    BasicBlock* exitBB = nullptr;
    // sub_switch state
    bool inGroup = false;
    BasicBlock* groupStartBB = nullptr;
    BasicBlock* groupEndBB = nullptr;
    AllocaInst* groupSel = nullptr;
    SwitchInst* groupSw = nullptr;
    int nextGroupId = 0;
    int64_t nextCaseVal = 0;  // "no case number causes next higher int case"
    bool sawDefault = false;
};

class CG {
public:
    CG(Program& prog, const std::string& name, bool aotMode) : prog_(prog), aot_(aotMode) {
        ctx_ = std::make_unique<LLVMContext>();
        mod_ = std::make_unique<Module>(name, *ctx_);
        i64_ = llvm::Type::getInt64Ty(*ctx_);
        i32_ = llvm::Type::getInt32Ty(*ctx_);
        f64_ = llvm::Type::getDoubleTy(*ctx_);
        ptr_ = PointerType::get(*ctx_, 0);
        voidTy_ = llvm::Type::getVoidTy(*ctx_);
    }

    CodegenResult run();

private:
    Program& prog_;
    bool aot_;
    std::unique_ptr<LLVMContext> ctx_;
    std::unique_ptr<Module> mod_;
    llvm::Type *i64_, *i32_, *f64_, *ptr_, *voidTy_;
    bool hadError_ = false;

    // ---- per-function generation context
    struct FnCtx {
        Function* fn = nullptr;
        std::unique_ptr<IRBuilder<>> b;
        Instruction* allocaMarker = nullptr;  // insert allocas before this
        std::vector<std::map<std::string, VarSym>> scopes;
        std::map<std::string, BasicBlock*> labels;
        std::set<std::string> labelsDefined;
        std::vector<BreakTarget> breaks;
        const FuncDecl* decl = nullptr;  // null for startup
        bool isVariadic = false;
        int retIsF64 = 0;  // 0 int, 1 f64, 2 void
    };
    FnCtx* fc_ = nullptr;  // current
    std::unique_ptr<FnCtx> startup_;

    std::map<std::string, FuncSym> funcs_;
    std::map<const FuncDecl*, Function*> declToFn_;
    std::map<std::string, VarSym> globals_;
    std::map<std::string, GlobalVariable*> strLits_;
    std::set<std::string> bodyDeclared_;
    int uniq_ = 0;

    IRBuilder<>& b() { return *fc_->b; }

    void error(const SrcLoc& loc, const std::string& msg) {
        fprintf(stderr, "%s: error: %s\n", loc.str().c_str(), msg.c_str());
        hadError_ = true;
    }

    // ---------------------------------------------------------- helpers
    Constant* cI64(int64_t v) { return ConstantInt::get(i64_, (uint64_t)v, true); }

    llvm::Type* memTy(const TypePtr& t) {
        // natural in-memory type for scalars; aggregates are byte arrays
        switch (t->kind) {
            case Type::F64:
                return f64_;
            case Type::Int:
                return llvm::Type::getIntNTy(*ctx_, t->intSize ? t->intSize * 8 : 8);
            case Type::Ptr:
            case Type::Func:
                return i64_;
            case Type::Void:
                return llvm::Type::getInt8Ty(*ctx_);
            case Type::Array:
            case Type::Class:
                return ArrayType::get(llvm::Type::getInt8Ty(*ctx_),
                                      (uint64_t)std::max<int64_t>(t->size(), 1));
        }
        return i64_;
    }

    bool isAggregate(const TypePtr& t) { return t->kind == Type::Array || t->kind == Type::Class; }

    Value* asPtr(Value* v) {
        if (v->getType()->isPointerTy()) return v;
        return b().CreateIntToPtr(v, ptr_);
    }
    Value* ptrToI64(Value* v) {
        if (v->getType()->isPointerTy()) return b().CreatePtrToInt(v, i64_);
        return v;
    }

    // widen a loaded scalar to the i64/double rvalue model
    Value* widen(Value* v, const TypePtr& t) {
        if (t->isF64()) return v;
        if (v->getType()->isPointerTy()) return b().CreatePtrToInt(v, i64_);
        if (v->getType() == i64_) return v;
        if (t->isInt() && t->isUnsigned) return b().CreateZExt(v, i64_);
        return b().CreateSExt(v, i64_);
    }

    // convert an rvalue to the given HolyC type's rvalue form (i64/double),
    // normalizing narrow ints ("resulting i1 is 0x5678")
    Value* convert(RV rv, const TypePtr& to, bool normalizeNarrow = true) {
        if (!rv.v)  // void (U0) value: reads as 0, TempleOS-forgiving
            return to->isF64() ? (Value*)ConstantFP::get(f64_, 0.0) : (Value*)cI64(0);
        bool srcF = rv.v->getType()->isDoubleTy();
        if (to->isF64()) {
            if (srcF) return rv.v;
            Value* iv = ptrToI64(rv.v);
            return rv.t && rv.t->isInt() && rv.t->isUnsigned && rv.t->intSize == 8
                       ? b().CreateUIToFP(iv, f64_)
                       : b().CreateSIToFP(iv, f64_);
        }
        Value* iv = srcF ? b().CreateFPToSI(rv.v, i64_) : ptrToI64(rv.v);
        if (normalizeNarrow && to->isInt() && to->intSize > 0 && to->intSize < 8) {
            llvm::Type* nt = llvm::Type::getIntNTy(*ctx_, to->intSize * 8);
            Value* tr = b().CreateTrunc(iv, nt);
            return to->isUnsigned ? b().CreateZExt(tr, i64_) : b().CreateSExt(tr, i64_);
        }
        return iv;
    }

    // store an rvalue into memory of type t
    void storeTo(Value* addr, RV rv, const TypePtr& t) {
        if (t->kind == Type::Class && t->cls && t->cls->wholeType) {
            // typed union ("I64i union I64") assigned as a whole
            storeTo(addr, rv, t->cls->wholeType);
            return;
        }
        llvm::Type* mt = memTy(t);
        if (isAggregate(t)) {
            // aggregate = aggregate copy (rv must carry an address)
            Value* src = asPtr(rv.v);
            b().CreateMemCpy(addr, MaybeAlign(1), src, MaybeAlign(1), (uint64_t)t->size());
            return;
        }
        Value* v;
        if (mt->isDoubleTy()) {
            v = convert(rv, tyF64());
        } else {
            Value* iv = convert(rv, tyI64(), false);
            v = mt == i64_ ? iv : b().CreateTrunc(iv, mt);
        }
        b().CreateStore(v, addr);
    }

    RV loadFrom(Value* addr, const TypePtr& t, const SrcLoc& loc) {
        if (t->kind == Type::Array) {
            // arrays decay to a pointer rvalue
            return {b().CreatePtrToInt(addr, i64_), tyPtr(t->elem)};
        }
        if (t->kind == Type::Class) {
            if (t->cls && t->cls->wholeType)  // typed union used whole
                return loadFrom(addr, t->cls->wholeType, loc);
            // class rvalue: its address (lets classes pass by reference)
            return {b().CreatePtrToInt(addr, i64_), tyPtr(t)};
        }
        Value* v = b().CreateLoad(memTy(t), addr);
        return {widen(v, t), t->isF64() ? tyF64() : t};
    }

    Value* truthy(RV rv, const SrcLoc& loc) {
        if (!rv.v) {
            error(loc, "void value used in condition");
            return ConstantInt::getFalse(*ctx_);
        }
        if (rv.v->getType()->isDoubleTy())
            return b().CreateFCmpONE(rv.v, ConstantFP::get(f64_, 0.0));
        return b().CreateICmpNE(ptrToI64(rv.v), cI64(0));
    }

    Value* boolToI64(Value* i1) { return b().CreateZExt(i1, i64_); }

    GlobalVariable* stringConst(const std::string& s) {
        auto it = strLits_.find(s);
        if (it != strLits_.end()) return it->second;
        auto* g = new GlobalVariable(
            *mod_, ArrayType::get(llvm::Type::getInt8Ty(*ctx_), s.size() + 1), true,
            GlobalValue::PrivateLinkage, ConstantDataArray::getString(*ctx_, s, true),
            ".str" + std::to_string(uniq_++));
        g->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        strLits_[s] = g;
        return g;
    }

    RV strRV(const std::string& s) {
        return {b().CreatePtrToInt(stringConst(s), i64_), tyPtr(tyU8())};
    }

    AllocaInst* entryAlloca(llvm::Type* ty, const std::string& name) {
        IRBuilder<> eb(fc_->allocaMarker);
        AllocaInst* a = eb.CreateAlloca(ty, nullptr, name);
        a->setAlignment(Align(8));
        return a;
    }

    VarSym* lookupVar(const std::string& n) {
        for (auto it = fc_->scopes.rbegin(); it != fc_->scopes.rend(); ++it) {
            auto f = it->find(n);
            if (f != it->end()) return &f->second;
        }
        auto g = globals_.find(n);
        if (g != globals_.end()) return &g->second;
        return nullptr;
    }

    // ---------------------------------------------------------- const eval
    bool evalConst(const Expr* e, int64_t* out) {
        if (!e) return false;
        switch (e->kind) {
            case Ex::IntLit:
            case Ex::CharLit:
                *out = e->intVal;
                return true;
            case Ex::FloatLit:
                *out = (int64_t)e->fltVal;
                return true;
            case Ex::SizeofType:
                *out = e->castType->size();
                return true;
            case Ex::OffsetOf: {
                auto ci = prog_.classes.find(e->className);
                if (ci == prog_.classes.end()) return false;
                const ClassMember* m = ci->second->findMember(e->memberName);
                if (!m) return false;
                *out = m->offset;
                return true;
            }
            case Ex::Cast: {
                if (!evalConst(e->kids[0].get(), out)) return false;
                const TypePtr& t = e->castType;
                if (t->isInt() && t->intSize > 0 && t->intSize < 8) {
                    int bits = t->intSize * 8;
                    uint64_t mask = (~0ULL) >> (64 - bits);
                    uint64_t v = (uint64_t)*out & mask;
                    if (!t->isUnsigned && (v >> (bits - 1))) v |= ~mask;
                    *out = (int64_t)v;
                }
                return true;
            }
            case Ex::Unary: {
                int64_t v;
                if (!e->prefix || !evalConst(e->kids[0].get(), &v)) return false;
                switch (e->punct) {
                    case P::Minus:
                        *out = -v;
                        return true;
                    case P::Plus:
                        *out = v;
                        return true;
                    case P::Tilde:
                        *out = ~v;
                        return true;
                    case P::Not:
                        *out = !v;
                        return true;
                    default:
                        return false;
                }
            }
            case Ex::Binary: {
                int64_t a, c;
                if (!evalConst(e->kids[0].get(), &a) || !evalConst(e->kids[1].get(), &c))
                    return false;
                switch (e->punct) {
                    case P::Plus:
                        *out = a + c;
                        return true;
                    case P::Minus:
                        *out = a - c;
                        return true;
                    case P::Star:
                        *out = a * c;
                        return true;
                    case P::Slash:
                        if (!c) return false;
                        *out = a / c;
                        return true;
                    case P::Percent:
                        if (!c) return false;
                        *out = a % c;
                        return true;
                    case P::Shl:
                        *out = (int64_t)((uint64_t)a << (c & 63));
                        return true;
                    case P::Shr:
                        *out = a >> (c & 63);
                        return true;
                    case P::Amp:
                        *out = a & c;
                        return true;
                    case P::Pipe:
                        *out = a | c;
                        return true;
                    case P::Caret:
                        *out = a ^ c;
                        return true;
                    case P::AndAnd:
                        *out = a && c;
                        return true;
                    case P::OrOr:
                        *out = a || c;
                        return true;
                    case P::XorXor:
                        *out = (!!a) ^ (!!c);
                        return true;
                    case P::EqEq:
                        *out = a == c;
                        return true;
                    case P::Ne:
                        *out = a != c;
                        return true;
                    case P::Lt:
                        *out = a < c;
                        return true;
                    case P::Gt:
                        *out = a > c;
                        return true;
                    case P::Le:
                        *out = a <= c;
                        return true;
                    case P::Ge:
                        *out = a >= c;
                        return true;
                    case P::Pow: {
                        int64_t r = 1;
                        while (c-- > 0) r *= a;
                        *out = r;
                        return true;
                    }
                    default:
                        return false;
                }
            }
            default:
                return false;
        }
    }

    bool evalConstF64(const Expr* e, double* out) {
        if (!e) return false;
        if (e->kind == Ex::FloatLit) {
            *out = e->fltVal;
            return true;
        }
        if (e->kind == Ex::Unary && e->punct == P::Minus) {
            if (!evalConstF64(e->kids[0].get(), out)) return false;
            *out = -*out;
            return true;
        }
        int64_t i;
        if (evalConst(e, &i)) {
            *out = (double)i;
            return true;
        }
        return false;
    }

    // ---------------------------------------------------------- types of exprs
    // best-effort static type (for sizeof(expr) and lastclass)
    TypePtr typeOf(const Expr* e) {
        if (!e) return tyI64();
        switch (e->kind) {
            case Ex::IntLit:
            case Ex::CharLit:
            case Ex::SizeofType:
            case Ex::SizeofExpr:
            case Ex::OffsetOf:
                return tyI64();
            case Ex::FloatLit:
                return tyF64();
            case Ex::StrLit:
                return tyPtr(tyU8());
            case Ex::Cast:
                return e->castType;
            case Ex::Ident: {
                if (VarSym* v = lookupVar(e->strVal)) return v->type;
                auto f = funcs_.find(e->strVal);
                if (f != funcs_.end()) return f->second.type->ret;
                return tyI64();
            }
            case Ex::Unary:
                if (e->punct == P::Amp) return tyPtr(typeOf(e->kids[0].get()));
                if (e->punct == P::Star) {
                    TypePtr t = typeOf(e->kids[0].get());
                    return t->isPtr() ? t->elem : tyU8();
                }
                if (e->punct == P::Not) return tyI64();
                return typeOf(e->kids[0].get());
            case Ex::Binary: {
                TypePtr a = typeOf(e->kids[0].get());
                TypePtr c = typeOf(e->kids[1].get());
                if (a->isF64() || c->isF64()) return tyF64();
                if (a->isPtr()) return a;
                if (c->isPtr()) return c;
                return tyI64();
            }
            case Ex::Assign:
                return typeOf(e->kids[0].get());
            case Ex::Index: {
                TypePtr t = typeOf(e->kids[0].get());
                if (t->isPtr()) return t->elem;
                if (t->kind == Type::Array) return t->elem;
                return tyU8();
            }
            case Ex::Member: {
                TypePtr bt = typeOf(e->kids[0].get());
                if (e->isArrow && bt->isPtr()) bt = bt->elem;
                if (bt->kind == Type::Class && bt->cls) {
                    const ClassMember* m = bt->cls->findMember(e->strVal);
                    if (m) return m->type;
                }
                TypePtr sub = subIntType(e->strVal);
                if (sub) return sub;
                return tyI64();
            }
            case Ex::Call: {
                const Expr* callee = e->kids[0].get();
                if (callee->kind == Ex::Ident) {
                    auto f = funcs_.find(callee->strVal);
                    if (f != funcs_.end()) return f->second.type->ret;
                    if (callee->strVal == "ToF64") return tyF64();
                }
                TypePtr ct = typeOf(callee);
                if (ct->kind == Type::Func && ct->ret) return ct->ret;
                if (ct->isPtr() && ct->elem->kind == Type::Func) return ct->elem->ret;
                return tyI64();
            }
            default:
                return tyI64();
        }
    }

    static TypePtr subIntType(const std::string& n) {
        if (n == "i8") return tyInt(1, false);
        if (n == "u8") return tyInt(1, true);
        if (n == "i16") return tyInt(2, false);
        if (n == "u16") return tyInt(2, true);
        if (n == "i32") return tyInt(4, false);
        if (n == "u32") return tyInt(4, true);
        if (n == "i64") return tyInt(8, false);
        if (n == "u64") return tyInt(8, true);
        return nullptr;
    }

    static std::string classNameOf(TypePtr t) {
        while (t && t->isPtr()) t = t->elem;
        if (!t) return "I64";
        if (t->kind == Type::Class && t->cls) return t->cls->name;
        return t->str();
    }

    // ---------------------------------------------------------- functions
    FunctionType* llvmFnType(const TypePtr& ft) {
        llvm::Type* ret = ft->ret->isF64() ? f64_ : ft->ret->kind == Type::Void ? voidTy_ : i64_;
        std::vector<llvm::Type*> ps;
        for (auto& p : *ft->params) ps.push_back(p.type && p.type->isF64() ? f64_ : i64_);
        if (ft->variadic) {
            ps.push_back(i64_);  // argc
            ps.push_back(i64_);  // argv
        }
        return FunctionType::get(ret, ps, false);
    }

    Function* declareFunc(const FuncDecl& fd) {
        std::string sym = fd.name;
        GlobalValue::LinkageTypes lk = GlobalValue::ExternalLinkage;
        switch (fd.linkage) {
            case Linkage::ExternAlias:
            case Linkage::ImportAlias:
                sym = fd.aliasSym;
                break;
            case Linkage::Static:
                lk = GlobalValue::InternalLinkage;
                break;
            default:
                break;
        }
        FunctionType* fty = llvmFnType(fd.type);
        Function* f = mod_->getFunction(sym);
        if (f && f->getFunctionType() != fty) f = nullptr;  // overshadow
        if (f && fd.hasBody && bodyDeclared_.count(sym))
            f = nullptr;  // dup definition: "a dup overshadows the original"
        if (!f) {
            std::string name = sym;
            if (mod_->getFunction(sym)) name = sym + "." + std::to_string(uniq_++);
            f = Function::Create(fty, lk, name, mod_.get());
        }
        if (fd.hasBody) bodyDeclared_.insert(sym);
        return f;
    }

    // ---------------------------------------------------------- expr codegen
    RV genExpr(const Expr* e) {
        switch (e->kind) {
            case Ex::IntLit:
                return {cI64(e->intVal), tyI64()};
            case Ex::CharLit:
                return {cI64(e->intVal), tyI64()};
            case Ex::FloatLit:
                return {ConstantFP::get(f64_, e->fltVal), tyF64()};
            case Ex::StrLit:
                return strRV(e->strVal);
            case Ex::SizeofType:
                return {cI64(e->castType->size()), tyI64()};
            case Ex::SizeofExpr:
                return {cI64(typeOf(e->kids[0].get())->size()), tyI64()};
            case Ex::OffsetOf: {
                int64_t v = 0;
                if (!evalConst(e, &v)) error(e->loc, "bad offset() expression");
                return {cI64(v), tyI64()};
            }
            case Ex::Ident:
                return genIdent(e);
            case Ex::Unary:
                return genUnary(e);
            case Ex::Binary:
                return genBinary(e);
            case Ex::ChainCmp:
                return genChainCmp(e);
            case Ex::Assign:
                return genAssign(e);
            case Ex::Call:
                return genCall(e);
            case Ex::Index:
            case Ex::Member: {
                LV lv = genLValue(e);
                if (!lv.addr) return {cI64(0), tyI64()};
                return loadFrom(lv.addr, lv.t, e->loc);
            }
            case Ex::Cast: {
                RV v = genExpr(e->kids[0].get());
                const TypePtr& to = e->castType;
                if (to->kind == Type::Void) return {nullptr, tyVoid()};
                if (isAggregate(to)) {
                    // value-cast to class: reinterpret address
                    return {v.v ? ptrToI64(v.v) : cI64(0), tyPtr(to)};
                }
                Value* out = to->isF64() ? convert(v, tyF64()) : convert(v, to);
                return {out, to};
            }
            case Ex::LastClass:
                error(e->loc, "'lastclass' is only valid as a default argument");
                return {cI64(0), tyI64()};
            case Ex::InitList:
                error(e->loc, "initializer list is only valid in declarations");
                return {cI64(0), tyI64()};
            case Ex::NoParenCall:
                break;
        }
        error(e->loc, "cannot generate expression");
        return {cI64(0), tyI64()};
    }

    RV genIdent(const Expr* e) {
        if (VarSym* v = lookupVar(e->strVal)) return loadFrom(v->addr, v->type, e->loc);
        auto f = funcs_.find(e->strVal);
        if (f != funcs_.end()) {
            // "Function with no args, or just default args can be called
            //  without parentheses."
            return genCallTo(f->second, {}, {}, e->loc);
        }
        error(e->loc, "undefined symbol '" + e->strVal + "'");
        return {cI64(0), tyI64()};
    }

    LV genLValue(const Expr* e) {
        switch (e->kind) {
            case Ex::Ident: {
                if (VarSym* v = lookupVar(e->strVal)) return {v->addr, v->type};
                if (funcs_.count(e->strVal)) {
                    error(e->loc, "function '" + e->strVal + "' is not an lvalue");
                    return {};
                }
                error(e->loc, "undefined symbol '" + e->strVal + "'");
                return {};
            }
            case Ex::Unary:
                if (e->punct == P::Star && e->prefix) {
                    RV p = genExpr(e->kids[0].get());
                    TypePtr elem = p.t->isPtr() ? p.t->elem : tyU8();
                    if (p.t->isPtr() && p.t->elem->kind == Type::Func)
                        break;  // deref of fn ptr is not an lvalue
                    return {asPtr(p.v), elem};
                }
                if (e->punct == P::PlusPlus || e->punct == P::MinusMinus) {
                    genUnary(e);  // side effect; ++x as lvalue is rare
                    return genLValue(e->kids[0].get());
                }
                break;
            case Ex::Index:
                return genIndexLV(e);
            case Ex::Member:
                return genMemberLV(e);
            case Ex::Cast: {
                // cast of an lvalue reinterprets in place: i(U8*) as lvalue
                LV lv = genLValue(e->kids[0].get());
                if (lv.addr) return {lv.addr, e->castType};
                return {};
            }
            case Ex::Assign: {
                genExpr(e);
                return genLValue(e->kids[0].get());
            }
            default:
                break;
        }
        error(e->loc, "expression is not an lvalue");
        return {};
    }

    LV genIndexLV(const Expr* e) {
        const Expr* base = e->kids[0].get();
        const Expr* idx = e->kids[1].get();
        // sub-int access: q.u8[5], q.i32[1].u8[2] ...
        if (base->kind == Ex::Member) {
            TypePtr sub = subIntType(base->strVal);
            if (sub && !base->isArrow) {
                LV blv = genLValue(base->kids[0].get());
                if (blv.addr && (blv.t->isInt() || blv.t->isF64())) {
                    RV iv = genExpr(idx);
                    Value* off = b().CreateMul(convert(iv, tyI64()), cI64(sub->intSize));
                    Value* a = b().CreateGEP(llvm::Type::getInt8Ty(*ctx_), blv.addr, off);
                    return {a, sub};
                }
                // else: fall through (a real class member named u8 etc.)
            }
        }
        RV bv;
        TypePtr elem;
        LV blv;
        TypePtr bt = typeOf(base);
        if (bt->kind == Type::Array) {
            blv = genLValue(base);
            if (!blv.addr) return {};
            bv = {b().CreatePtrToInt(blv.addr, i64_), tyPtr(bt->elem)};
            elem = bt->elem;
        } else {
            bv = genExpr(base);
            elem = bv.t->isPtr() ? bv.t->elem : tyU8();
        }
        if (elem->kind == Type::Void) elem = tyU8();
        RV iv = genExpr(idx);
        int64_t esz = std::max<int64_t>(elem->size(), 1);
        Value* off = b().CreateMul(convert(iv, tyI64()), cI64(esz));
        Value* addr = b().CreateAdd(ptrToI64(bv.v), off);
        return {asPtr(addr), elem};
    }

    LV genMemberLV(const Expr* e) {
        const Expr* base = e->kids[0].get();
        Value* baseAddr = nullptr;
        TypePtr bt;
        if (e->isArrow) {
            RV p = genExpr(base);
            bt = p.t->isPtr() ? p.t->elem : nullptr;
            baseAddr = asPtr(p.v);
        } else {
            LV lv = genLValue(base);
            if (!lv.addr) return {};
            bt = lv.t;
            baseAddr = lv.addr;
        }
        if (bt && bt->kind == Type::Class && bt->cls) {
            const ClassMember* m = bt->cls->findMember(e->strVal);
            if (m) {
                Value* a = b().CreateGEP(llvm::Type::getInt8Ty(*ctx_), baseAddr, cI64(m->offset));
                return {a, m->type};
            }
            error(e->loc, "no member '" + e->strVal + "' in class '" + bt->cls->name + "'");
            return {};
        }
        // sub-int member without index: q.u8 == q.u8[0]
        if (bt && (bt->isInt() || bt->isF64())) {
            TypePtr sub = subIntType(e->strVal);
            if (sub) return {baseAddr, sub};
        }
        error(e->loc,
              "member access on non-class value" + (bt ? " of type " + bt->str() : std::string()));
        return {};
    }

    RV genUnary(const Expr* e) {
        const Expr* k = e->kids[0].get();
        switch (e->punct) {
            case P::Amp: {
                // &Fun -> function address
                if (k->kind == Ex::Ident) {
                    auto f = funcs_.find(k->strVal);
                    if (f != funcs_.end() && !lookupVar(k->strVal)) {
                        return {b().CreatePtrToInt(f->second.fn, i64_), tyPtr(f->second.type)};
                    }
                }
                LV lv = genLValue(k);
                if (!lv.addr) return {cI64(0), tyI64()};
                return {b().CreatePtrToInt(lv.addr, i64_), tyPtr(lv.t)};
            }
            case P::Star: {
                RV p = genExpr(k);
                // deref of a fn ptr yields the fn value itself: (*fp)(...)
                if (p.t->isPtr() && p.t->elem->kind == Type::Func) return p;
                if (p.t->kind == Type::Func) return p;
                TypePtr elem = p.t->isPtr() ? p.t->elem : tyU8();
                if (elem->kind == Type::Void) elem = tyU8();
                return loadFrom(asPtr(p.v), elem, e->loc);
            }
            case P::Not: {
                RV v = genExpr(k);
                Value* z = truthy(v, e->loc);
                return {boolToI64(b().CreateNot(z)), tyI64()};
            }
            case P::Tilde: {
                RV v = genExpr(k);
                return {b().CreateXor(convert(v, tyI64(), false), cI64(-1)), tyI64()};
            }
            case P::Minus: {
                RV v = genExpr(k);
                if (v.v && v.v->getType()->isDoubleTy()) return {b().CreateFNeg(v.v), tyF64()};
                return {b().CreateNeg(convert(v, tyI64(), false)), v.t->isInt() ? v.t : tyI64()};
            }
            case P::Plus:
                return genExpr(k);
            case P::PlusPlus:
            case P::MinusMinus: {
                LV lv = genLValue(k);
                if (!lv.addr) return {cI64(0), tyI64()};
                RV old = loadFrom(lv.addr, lv.t, e->loc);
                Value* nv;
                if (old.v->getType()->isDoubleTy()) {
                    Value* one = ConstantFP::get(f64_, 1.0);
                    nv = e->punct == P::PlusPlus ? b().CreateFAdd(old.v, one)
                                                 : b().CreateFSub(old.v, one);
                } else {
                    // pointers advance by ONE BYTE (unscaled, "just I64s")
                    nv = e->punct == P::PlusPlus ? b().CreateAdd(old.v, cI64(1))
                                                 : b().CreateSub(old.v, cI64(1));
                }
                storeTo(lv.addr, {nv, old.t}, lv.t);
                return {e->prefix ? nv : old.v, old.t};
            }
            default:
                break;
        }
        error(e->loc, "bad unary operator");
        return {cI64(0), tyI64()};
    }

    static bool isU64(const TypePtr& t) {
        return t && ((t->isInt() && t->isUnsigned && t->intSize == 8) || t->isPtr());
    }

    RV arith(P op, RV a, RV c, const SrcLoc& loc) {
        bool f = (a.v && a.v->getType()->isDoubleTy()) || (c.v && c.v->getType()->isDoubleTy());
        if (f && (op == P::Amp || op == P::Pipe || op == P::Caret || op == P::Shl || op == P::Shr))
            f = false;  // bitwise ops force integer domain
        if (f) {
            Value* x = convert(a, tyF64());
            Value* y = convert(c, tyF64());
            switch (op) {
                case P::Plus:
                    return {b().CreateFAdd(x, y), tyF64()};
                case P::Minus:
                    return {b().CreateFSub(x, y), tyF64()};
                case P::Star:
                    return {b().CreateFMul(x, y), tyF64()};
                case P::Slash:
                    return {b().CreateFDiv(x, y), tyF64()};
                case P::Percent:
                    return {b().CreateFRem(x, y), tyF64()};
                case P::Pow: {
                    Function* powf = getRT("HC_Pow", f64_, {f64_, f64_});
                    return {b().CreateCall(powf, {x, y}), tyF64()};
                }
                default:
                    break;
            }
            error(loc, "bad float operation");
            return {ConstantFP::get(f64_, 0.0), tyF64()};
        }
        Value* x = convert(a, tyI64(), false);
        Value* y = convert(c, tyI64(), false);
        bool uns = isU64(a.t) || isU64(c.t);
        // pointer +/- stays typed as the pointer (unscaled byte math)
        TypePtr rt = tyI64();
        if (a.t && a.t->isPtr())
            rt = a.t;
        else if (c.t && c.t->isPtr())
            rt = c.t;
        else if (uns)
            rt = tyU64();
        switch (op) {
            case P::Plus:
                return {b().CreateAdd(x, y), rt};
            case P::Minus:
                return {b().CreateSub(x, y), rt};
            case P::Star:
                return {b().CreateMul(x, y), rt};
            case P::Slash:
                return {uns ? b().CreateUDiv(x, y) : b().CreateSDiv(x, y), rt};
            case P::Percent:
                return {uns ? b().CreateURem(x, y) : b().CreateSRem(x, y), rt};
            case P::Amp:
                return {b().CreateAnd(x, y), rt};
            case P::Pipe:
                return {b().CreateOr(x, y), rt};
            case P::Caret:
                return {b().CreateXor(x, y), rt};
            case P::Shl:
                // x86/HolyC mask shift count to 6 bits (SHL masks to &0x3F);
                // avoids LLVM poison for counts >= 64
                return {b().CreateShl(x, b().CreateAnd(y, cI64(63))), rt};
            case P::Shr:
                // U64 >> is logical; I64 >> is arithmetic (doc example)
                y = b().CreateAnd(y, cI64(63));
                return {isU64(a.t) ? b().CreateLShr(x, y) : b().CreateAShr(x, y), rt};
            case P::Pow: {
                Function* powi = getRT("HC_PowI64", i64_, {i64_, i64_});
                return {b().CreateCall(powi, {x, y}), tyI64()};
            }
            default:
                break;
        }
        error(loc, "bad integer operation");
        return {cI64(0), tyI64()};
    }

    Value* cmp(P op, RV a, RV c) {
        bool f = (a.v && a.v->getType()->isDoubleTy()) || (c.v && c.v->getType()->isDoubleTy());
        if (f) {
            Value* x = convert(a, tyF64());
            Value* y = convert(c, tyF64());
            switch (op) {
                case P::Lt:
                    return b().CreateFCmpOLT(x, y);
                case P::Gt:
                    return b().CreateFCmpOGT(x, y);
                case P::Le:
                    return b().CreateFCmpOLE(x, y);
                case P::Ge:
                    return b().CreateFCmpOGE(x, y);
                case P::EqEq:
                    return b().CreateFCmpOEQ(x, y);
                case P::Ne:
                    return b().CreateFCmpONE(x, y);
                default:
                    return ConstantInt::getFalse(*ctx_);
            }
        }
        Value* x = convert(a, tyI64(), false);
        Value* y = convert(c, tyI64(), false);
        bool uns = isU64(a.t) || isU64(c.t);
        switch (op) {
            case P::Lt:
                return uns ? b().CreateICmpULT(x, y) : b().CreateICmpSLT(x, y);
            case P::Gt:
                return uns ? b().CreateICmpUGT(x, y) : b().CreateICmpSGT(x, y);
            case P::Le:
                return uns ? b().CreateICmpULE(x, y) : b().CreateICmpSLE(x, y);
            case P::Ge:
                return uns ? b().CreateICmpUGE(x, y) : b().CreateICmpSGE(x, y);
            case P::EqEq:
                return b().CreateICmpEQ(x, y);
            case P::Ne:
                return b().CreateICmpNE(x, y);
            default:
                return ConstantInt::getFalse(*ctx_);
        }
    }

    RV genBinary(const Expr* e) {
        P op = e->punct;
        if (op == P::AndAnd || op == P::OrOr) return genShortCircuit(e);
        if (op == P::XorXor) {
            RV a = genExpr(e->kids[0].get());
            RV c = genExpr(e->kids[1].get());
            Value* x = truthy(a, e->loc);
            Value* y = truthy(c, e->loc);
            return {boolToI64(b().CreateXor(x, y)), tyI64()};
        }
        if (op == P::EqEq || op == P::Ne) {
            RV a = genExpr(e->kids[0].get());
            RV c = genExpr(e->kids[1].get());
            return {boolToI64(cmp(op, a, c)), tyI64()};
        }
        RV a = genExpr(e->kids[0].get());
        RV c = genExpr(e->kids[1].get());
        return arith(op, a, c, e->loc);
    }

    RV genShortCircuit(const Expr* e) {
        bool isAnd = e->punct == P::AndAnd;
        Function* fn = fc_->fn;
        RV a = genExpr(e->kids[0].get());
        Value* av = truthy(a, e->loc);
        BasicBlock* lhsBB = b().GetInsertBlock();
        BasicBlock* rhsBB = BasicBlock::Create(*ctx_, "sc.rhs", fn);
        BasicBlock* endBB = BasicBlock::Create(*ctx_, "sc.end", fn);
        if (isAnd)
            b().CreateCondBr(av, rhsBB, endBB);
        else
            b().CreateCondBr(av, endBB, rhsBB);
        b().SetInsertPoint(rhsBB);
        RV c = genExpr(e->kids[1].get());
        Value* cv = boolToI64(truthy(c, e->loc));
        BasicBlock* rhsEnd = b().GetInsertBlock();
        b().CreateBr(endBB);
        b().SetInsertPoint(endBB);
        PHINode* phi = b().CreatePHI(i64_, 2);
        phi->addIncoming(cI64(isAnd ? 0 : 1), lhsBB);
        phi->addIncoming(cv, rhsEnd);
        return {phi, tyI64()};
    }

    // 5<i<j+1<20 with left-to-right single evaluation and short-circuit
    RV genChainCmp(const Expr* e) {
        Function* fn = fc_->fn;
        BasicBlock* endBB = BasicBlock::Create(*ctx_, "chain.end", fn);
        std::vector<std::pair<BasicBlock*, Value*>> incoming;
        RV prev = genExpr(e->kids[0].get());
        for (size_t i = 0; i < e->chainOps.size(); i++) {
            RV cur = genExpr(e->kids[i + 1].get());
            Value* c = cmp(e->chainOps[i], prev, cur);
            if (i + 1 == e->chainOps.size()) {
                incoming.push_back({b().GetInsertBlock(), boolToI64(c)});
                b().CreateBr(endBB);
            } else {
                BasicBlock* next = BasicBlock::Create(*ctx_, "chain.next", fn);
                incoming.push_back({b().GetInsertBlock(), cI64(0)});
                b().CreateCondBr(c, next, endBB);
                b().SetInsertPoint(next);
            }
            prev = cur;
        }
        b().SetInsertPoint(endBB);
        PHINode* phi = b().CreatePHI(i64_, (unsigned)incoming.size());
        for (auto& in : incoming) phi->addIncoming(in.second, in.first);
        return {phi, tyI64()};
    }

    RV genAssign(const Expr* e) {
        LV lv = genLValue(e->kids[0].get());
        RV rhs;
        if (e->punct == P::Assign) {
            rhs = genExpr(e->kids[1].get());
        } else {
            if (!lv.addr) {
                genExpr(e->kids[1].get());
                return {cI64(0), tyI64()};
            }
            RV old = loadFrom(lv.addr, lv.t, e->loc);
            RV rv = genExpr(e->kids[1].get());
            P op;
            switch (e->punct) {
                case P::PlusEq:
                    op = P::Plus;
                    break;
                case P::MinusEq:
                    op = P::Minus;
                    break;
                case P::StarEq:
                    op = P::Star;
                    break;
                case P::SlashEq:
                    op = P::Slash;
                    break;
                case P::PercentEq:
                    op = P::Percent;
                    break;
                case P::ShlEq:
                    op = P::Shl;
                    break;
                case P::ShrEq:
                    op = P::Shr;
                    break;
                case P::AmpEq:
                    op = P::Amp;
                    break;
                case P::PipeEq:
                    op = P::Pipe;
                    break;
                case P::CaretEq:
                    op = P::Caret;
                    break;
                default:
                    op = P::Plus;
                    break;
            }
            rhs = arith(op, old, rv, e->loc);
        }
        if (lv.addr) storeTo(lv.addr, rhs, lv.t);
        // "j1=i1=0x12345678: i1 is 0x5678 but j1 is 0x12345678" --
        // the assignment expression yields the UNtruncated value.
        return rhs;
    }

    // ---------------------------------------------------------- calls
    Function* getRT(const char* name, llvm::Type* ret, std::vector<llvm::Type*> args) {
        Function* f = mod_->getFunction(name);
        if (f) return f;
        return Function::Create(FunctionType::get(ret, args, false), GlobalValue::ExternalLinkage,
                                name, mod_.get());
    }

    RV genCall(const Expr* e) {
        const Expr* callee = e->kids[0].get();
        std::vector<const Expr*> args;
        std::vector<bool> present;
        for (size_t i = 1; i < e->kids.size(); i++) {
            args.push_back(e->kids[i].get());
            present.push_back(e->argPresent[i - 1]);
        }
        if (callee->kind == Ex::Ident && !lookupVar(callee->strVal)) {
            const std::string& name = callee->strVal;
            // compiler intrinsics: postfix-cast helpers
            if (name == "ToI64" || name == "ToBool" || name == "ToF64") {
                if (args.size() != 1 || !present[0]) {
                    error(e->loc, name + "() takes exactly one argument");
                    return {cI64(0), tyI64()};
                }
                RV v = genExpr(args[0]);
                if (name == "ToF64") return {convert(v, tyF64()), tyF64()};
                if (name == "ToBool") return {boolToI64(truthy(v, e->loc)), tyI64()};
                return {convert(v, tyI64()), tyI64()};
            }
            auto f = funcs_.find(name);
            if (f != funcs_.end()) return genCallTo(f->second, args, present, e->loc);
            error(e->loc, "undefined function '" + name + "'");
            return {cI64(0), tyI64()};
        }
        // indirect call through expression / function pointer
        RV fv = genExpr(callee);
        TypePtr ft = fv.t;
        if (ft->isPtr() && ft->elem->kind == Type::Func) ft = ft->elem;
        if (ft->kind == Type::Func) {
            FuncSym sym;
            sym.fn = nullptr;
            sym.type = ft;
            return genCallTo(sym, args, present, e->loc, fv.v);
        }
        // unknown signature: all-i64, non-variadic (HolyC has no type checking)
        std::vector<Value*> vs;
        std::vector<llvm::Type*> ts;
        for (size_t i = 0; i < args.size(); i++) {
            if (!present[i]) {
                error(e->loc, "cannot skip args when calling through an untyped pointer");
                vs.push_back(cI64(0));
                ts.push_back(i64_);
                continue;
            }
            RV a = genExpr(args[i]);
            if (a.v && a.v->getType()->isDoubleTy()) {
                vs.push_back(a.v);
                ts.push_back(f64_);
            } else {
                vs.push_back(convert(a, tyI64(), false));
                ts.push_back(i64_);
            }
        }
        FunctionType* fty = FunctionType::get(i64_, ts, false);
        Value* fp = asPtr(fv.v);
        return {b().CreateCall(fty, fp, vs), tyI64()};
    }

    RV genCallTo(const FuncSym& sym, std::vector<const Expr*> args, std::vector<bool> present,
                 const SrcLoc& loc, Value* fnPtr = nullptr) {
        const auto& params = *sym.type->params;
        bool variadic = sym.type->variadic;
        if (args.size() > params.size() && !variadic) {
            error(loc, "too many arguments");
            args.resize(params.size());
            present.resize(params.size());
        }
        std::vector<Value*> vs;
        TypePtr prevArgType;  // for lastclass
        for (size_t i = 0; i < params.size(); i++) {
            const FuncParam& p = params[i];
            RV a;
            if (i < args.size() && present[i]) {
                a = genExpr(args[i]);
            } else if (p.dfltIsLastClass) {
                a = strRV(classNameOf(prevArgType ? prevArgType : tyI64()));
            } else if (p.dflt) {
                a = genExpr(p.dflt.get());
            } else {
                error(loc, "missing argument " + std::to_string(i + 1) +
                               (p.name.empty() ? "" : " ('" + p.name + "')") +
                               " and no default value");
                a = {cI64(0), tyI64()};
            }
            prevArgType = (i < args.size() && present[i]) ? typeOf(args[i]) : a.t;
            if (p.type && p.type->isF64())
                vs.push_back(convert(a, tyF64()));
            else
                vs.push_back(convert(a, tyI64(), false));
        }
        if (variadic) {
            // extra args -> I64 argv[] array (F64s bit-cast into slots)
            std::vector<Value*> extra;
            for (size_t i = params.size(); i < args.size(); i++) {
                if (!present[i]) {
                    error(loc, "missing variadic argument has no default");
                    extra.push_back(cI64(0));
                    continue;
                }
                RV a = genExpr(args[i]);
                if (a.v && a.v->getType()->isDoubleTy())
                    extra.push_back(b().CreateBitCast(a.v, i64_));
                else
                    extra.push_back(convert(a, tyI64(), false));
            }
            int64_t n = (int64_t)extra.size();
            Value* arr;
            if (n) {
                AllocaInst* al = entryAlloca(ArrayType::get(i64_, (uint64_t)n), "va");
                for (int64_t i = 0; i < n; i++) {
                    Value* slot =
                        b().CreateGEP(ArrayType::get(i64_, (uint64_t)n), al, {cI64(0), cI64(i)});
                    b().CreateStore(extra[(size_t)i], slot);
                }
                arr = b().CreatePtrToInt(al, i64_);
            } else {
                arr = cI64(0);
            }
            vs.push_back(cI64(n));
            vs.push_back(arr);
        }
        FunctionType* fty = llvmFnType(sym.type);
        CallInst* call;
        if (sym.fn)
            call = b().CreateCall(sym.fn, vs);
        else
            call = b().CreateCall(fty, asPtr(fnPtr), vs);
        const TypePtr& rt = sym.type->ret;
        if (rt->kind == Type::Void) return {nullptr, tyVoid()};
        return {call, rt->isF64() ? tyF64() : rt};
    }

    // ---------------------------------------------------------- statements
    void genStmt(const Stmt* s) {
        switch (s->kind) {
            case St::Empty:
                return;
            case St::Block: {
                fc_->scopes.emplace_back();
                for (auto& k : s->body) genStmt(k.get());
                fc_->scopes.pop_back();
                return;
            }
            case St::Expr:
                genExpr(s->expr.get());
                return;
            case St::If:
                return genIf(s);
            case St::While:
                return genWhile(s);
            case St::DoWhile:
                return genDoWhile(s);
            case St::For:
                return genFor(s);
            case St::Switch:
                return genSwitch(s);
            case St::Break: {
                if (fc_->breaks.empty()) {
                    error(s->loc, "break outside of loop or switch");
                    return;
                }
                BreakTarget& t = fc_->breaks.back();
                BasicBlock* dst = (t.swc && t.swc->inGroup) ? t.swc->groupEndBB : t.bb;
                b().CreateBr(dst);
                startDeadBlock("after.break");
                return;
            }
            case St::Goto: {
                BasicBlock* bb = getLabelBlock(s->label);
                b().CreateBr(bb);
                startDeadBlock("after.goto");
                return;
            }
            case St::Label: {
                BasicBlock* bb = getLabelBlock(s->label);
                fc_->labelsDefined.insert(s->label);
                if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(bb);
                b().SetInsertPoint(bb);
                return;
            }
            case St::Return:
                return genReturn(s);
            case St::VarDecl:
                return genVarDecl(s);
            case St::Try:
                return genTry(s);
            case St::NoWarn:
                return;
            case St::Lock:
                // best effort: compile the statement normally (single
                // threaded runtime; LOCK prefixes are a no-op here)
                for (auto& k : s->body) genStmt(k.get());
                return;
            case St::Case:
            case St::Default:
            case St::SubStart:
            case St::SubEnd:
                error(s->loc, "case labels are only valid directly inside a switch");
                return;
        }
    }

    void startDeadBlock(const char* name) {
        BasicBlock* dead = BasicBlock::Create(*ctx_, name, fc_->fn);
        b().SetInsertPoint(dead);
    }

    BasicBlock* getLabelBlock(const std::string& name) {
        auto it = fc_->labels.find(name);
        if (it != fc_->labels.end()) return it->second;
        BasicBlock* bb = BasicBlock::Create(*ctx_, ("lbl." + name), fc_->fn);
        fc_->labels[name] = bb;
        return bb;
    }

    void genIf(const Stmt* s) {
        Value* c = truthy(genExpr(s->expr.get()), s->loc);
        BasicBlock* thenBB = BasicBlock::Create(*ctx_, "if.then", fc_->fn);
        BasicBlock* endBB = BasicBlock::Create(*ctx_, "if.end", fc_->fn);
        BasicBlock* elseBB =
            s->body2.empty() ? endBB : BasicBlock::Create(*ctx_, "if.else", fc_->fn);
        b().CreateCondBr(c, thenBB, elseBB);
        b().SetInsertPoint(thenBB);
        genStmt(s->body[0].get());
        if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(endBB);
        if (!s->body2.empty()) {
            b().SetInsertPoint(elseBB);
            genStmt(s->body2[0].get());
            if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(endBB);
        }
        b().SetInsertPoint(endBB);
    }

    void genWhile(const Stmt* s) {
        BasicBlock* condBB = BasicBlock::Create(*ctx_, "while.cond", fc_->fn);
        BasicBlock* bodyBB = BasicBlock::Create(*ctx_, "while.body", fc_->fn);
        BasicBlock* endBB = BasicBlock::Create(*ctx_, "while.end", fc_->fn);
        b().CreateBr(condBB);
        b().SetInsertPoint(condBB);
        Value* c = truthy(genExpr(s->expr.get()), s->loc);
        b().CreateCondBr(c, bodyBB, endBB);
        b().SetInsertPoint(bodyBB);
        fc_->breaks.push_back({endBB, nullptr});
        genStmt(s->body[0].get());
        fc_->breaks.pop_back();
        if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(condBB);
        b().SetInsertPoint(endBB);
    }

    void genDoWhile(const Stmt* s) {
        BasicBlock* bodyBB = BasicBlock::Create(*ctx_, "do.body", fc_->fn);
        BasicBlock* condBB = BasicBlock::Create(*ctx_, "do.cond", fc_->fn);
        BasicBlock* endBB = BasicBlock::Create(*ctx_, "do.end", fc_->fn);
        b().CreateBr(bodyBB);
        b().SetInsertPoint(bodyBB);
        fc_->breaks.push_back({endBB, nullptr});
        genStmt(s->body[0].get());
        fc_->breaks.pop_back();
        if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(condBB);
        b().SetInsertPoint(condBB);
        Value* c = truthy(genExpr(s->expr.get()), s->loc);
        b().CreateCondBr(c, bodyBB, endBB);
        b().SetInsertPoint(endBB);
    }

    void genFor(const Stmt* s) {
        for (auto& e : s->exprs) genExpr(e.get());
        BasicBlock* condBB = BasicBlock::Create(*ctx_, "for.cond", fc_->fn);
        BasicBlock* bodyBB = BasicBlock::Create(*ctx_, "for.body", fc_->fn);
        BasicBlock* endBB = BasicBlock::Create(*ctx_, "for.end", fc_->fn);
        b().CreateBr(condBB);
        b().SetInsertPoint(condBB);
        if (s->expr2) {
            Value* c = truthy(genExpr(s->expr2.get()), s->loc);
            b().CreateCondBr(c, bodyBB, endBB);
        } else {
            b().CreateBr(bodyBB);
        }
        b().SetInsertPoint(bodyBB);
        fc_->breaks.push_back({endBB, nullptr});
        genStmt(s->body[0].get());
        fc_->breaks.pop_back();
        if (!b().GetInsertBlock()->getTerminator()) {
            for (auto& p : s->body2) genStmt(p.get());  // post exprs
            b().CreateBr(condBB);
        }
        b().SetInsertPoint(endBB);
    }

    void genReturn(const Stmt* s) {
        if (fc_->retIsF64 == 2) {
            if (s->expr) genExpr(s->expr.get());  // evaluate for side effects
            b().CreateRetVoid();
        } else if (fc_->retIsF64 == 1) {
            Value* v =
                s->expr ? convert(genExpr(s->expr.get()), tyF64()) : ConstantFP::get(f64_, 0.0);
            b().CreateRet(v);
        } else {
            Value* v = s->expr ? convert(genExpr(s->expr.get()), tyI64(), false) : cI64(0);
            b().CreateRet(v);
        }
        startDeadBlock("after.ret");
    }

    // ---------------------------------------------------------- switch
    void genSwitch(const Stmt* s) {
        Value* cond = convert(genExpr(s->expr.get()), tyI64(), false);
        Function* fn = fc_->fn;
        BasicBlock* exitBB = BasicBlock::Create(*ctx_, "sw.end", fn);
        BasicBlock* firstBB = BasicBlock::Create(*ctx_, "sw.dispatch", fn);
        // the switch instruction lives in its own block; body blocks follow
        b().CreateBr(firstBB);
        b().SetInsertPoint(firstBB);
        SwitchInst* sw = b().CreateSwitch(cond, exitBB);

        SwitchCtx swc;
        swc.sw = sw;
        swc.exitBB = exitBB;
        fc_->breaks.push_back({exitBB, &swc});

        // walk the body linearly; cases carve new blocks
        startDeadBlock("sw.preamble");  // stmts before first case are dead
        const Stmt* body = s->body[0].get();
        std::vector<const Stmt*> items;
        if (body->kind == St::Block)
            for (auto& k : body->body) items.push_back(k.get());
        else
            items.push_back(body);

        fc_->scopes.emplace_back();
        for (const Stmt* it : items) {
            switch (it->kind) {
                case St::Case:
                    genCaseLabel(it, swc);
                    break;
                case St::Default: {
                    BasicBlock* bb = BasicBlock::Create(*ctx_, "sw.default", fn);
                    if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(bb);
                    b().SetInsertPoint(bb);
                    if (swc.inGroup) {
                        error(it->loc, "default: inside start:/end: group");
                    } else {
                        sw->setDefaultDest(bb);
                        swc.sawDefault = true;
                    }
                    break;
                }
                case St::SubStart:
                    genSubStart(it, swc);
                    break;
                case St::SubEnd:
                    genSubEnd(it, swc);
                    break;
                default:
                    genStmt(it);
                    break;
            }
        }
        fc_->scopes.pop_back();
        fc_->breaks.pop_back();
        if (swc.inGroup) error(s->loc, "start: without matching end: in switch");
        if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(exitBB);
        b().SetInsertPoint(exitBB);
    }

    ConstantInt* cInt(llvm::Type* ty, int64_t v) {
        return ConstantInt::get(cast<IntegerType>(ty), (uint64_t)v, true);
    }

    void genCaseLabel(const Stmt* it, SwitchCtx& swc) {
        int64_t lo, hi;
        if (it->hasCaseVal) {
            if (!evalConst(it->caseLo.get(), &lo)) {
                error(it->loc, "case value must be a constant expression");
                lo = swc.nextCaseVal;
            }
            hi = lo;
            if (it->hasHi && !evalConst(it->caseHi.get(), &hi)) {
                error(it->loc, "case range end must be a constant expression");
                hi = lo;
            }
        } else {
            // "A no case number causes next higher int case"
            lo = hi = swc.nextCaseVal;
        }
        swc.nextCaseVal = hi + 1;
        if (hi < lo || hi - lo > 100000) {
            error(it->loc, "case range too large");
            hi = lo;
        }
        Function* fn = fc_->fn;
        BasicBlock* bb = BasicBlock::Create(*ctx_, "sw.case", fn);

        if (swc.inGroup) {
            if (!swc.groupSw) {
                // first case of the group: terminate the front porch with
                // the inner dispatch on the remembered case id
                if (!b().GetInsertBlock()->getTerminator()) {
                    Value* sel = b().CreateLoad(i32_, swc.groupSel);
                    swc.groupSw = b().CreateSwitch(sel, bb);
                } else {
                    error(it->loc, "start: front porch may not end in a jump");
                }
            } else if (!b().GetInsertBlock()->getTerminator()) {
                b().CreateBr(bb);  // fallthrough from previous case body
            }
            int id = swc.nextGroupId++;
            // dispatch stub: remember the target case, then run the porch
            BasicBlock* stub = BasicBlock::Create(*ctx_, "sw.substub", fn);
            IRBuilder<> sb(stub);
            sb.CreateStore(cInt(i32_, id), swc.groupSel);
            sb.CreateBr(swc.groupStartBB);
            for (int64_t v = lo; v <= hi; v++) swc.sw->addCase(cInt(i64_, v), stub);
            if (swc.groupSw) swc.groupSw->addCase(cInt(i32_, id), bb);
        } else {
            if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(bb);  // fallthrough
            for (int64_t v = lo; v <= hi; v++) swc.sw->addCase(cInt(i64_, v), bb);
        }
        b().SetInsertPoint(bb);
    }

    void genSubStart(const Stmt* it, SwitchCtx& swc) {
        if (swc.inGroup) {
            error(it->loc, "nested start: in switch");
            return;
        }
        Function* fn = fc_->fn;
        swc.inGroup = true;
        swc.groupStartBB = BasicBlock::Create(*ctx_, "sw.porch", fn);
        swc.groupEndBB = BasicBlock::Create(*ctx_, "sw.endporch", fn);
        swc.groupSel = entryAlloca(i32_, "sw.sel");
        swc.groupSw = nullptr;
        swc.nextGroupId = 0;
        // sequential fallthrough into the porch enters the first group case
        if (!b().GetInsertBlock()->getTerminator()) {
            b().CreateStore(ConstantInt::get(i32_, 0), swc.groupSel);
            b().CreateBr(swc.groupStartBB);
        }
        b().SetInsertPoint(swc.groupStartBB);
        // porch statements now emit here until the first case label
    }

    void genSubEnd(const Stmt* it, SwitchCtx& swc) {
        if (!swc.inGroup) {
            error(it->loc, "end: without start: in switch");
            return;
        }
        // last group case falls through into the end porch
        if (!b().GetInsertBlock()->getTerminator()) b().CreateBr(swc.groupEndBB);
        swc.inGroup = false;
        b().SetInsertPoint(swc.groupEndBB);
        // end porch statements emit here; a trailing break exits the switch
    }

    // ---------------------------------------------------------- try/catch
    void genTry(const Stmt* s) {
        Function* fn = fc_->fn;
        AllocaInst* frame =
            entryAlloca(ArrayType::get(llvm::Type::getInt8Ty(*ctx_), 256), "try.frame");
        Function* push = getRT("HC_TryPush", ptr_, {ptr_});
        Function* pop = getRT("HC_TryPop", voidTy_, {});
        Function* centr = getRT("HC_CatchEnter", voidTy_, {});
        Function* cdone = getRT("HC_CatchDone", voidTy_, {});
        Function* setjmpF = mod_->getFunction("setjmp");
        if (!setjmpF) {
            setjmpF = Function::Create(FunctionType::get(i32_, {ptr_}, false),
                                       GlobalValue::ExternalLinkage, "setjmp", mod_.get());
            setjmpF->addFnAttr(Attribute::ReturnsTwice);
        }
        Value* jb = b().CreateCall(push, {frame});
        Value* rc = b().CreateCall(setjmpF, {jb});
        cast<CallInst>(rc)->addFnAttr(Attribute::ReturnsTwice);
        BasicBlock* tryBB = BasicBlock::Create(*ctx_, "try.body", fn);
        BasicBlock* catchBB = BasicBlock::Create(*ctx_, "catch.body", fn);
        BasicBlock* endBB = BasicBlock::Create(*ctx_, "try.end", fn);
        b().CreateCondBr(b().CreateICmpEQ(rc, ConstantInt::get(i32_, 0)), tryBB, catchBB);

        b().SetInsertPoint(tryBB);
        fc_->scopes.emplace_back();
        for (auto& k : s->body) genStmt(k.get());
        fc_->scopes.pop_back();
        if (!b().GetInsertBlock()->getTerminator()) {
            b().CreateCall(pop, {});
            b().CreateBr(endBB);
        }

        b().SetInsertPoint(catchBB);
        b().CreateCall(centr, {});
        fc_->scopes.emplace_back();
        for (auto& k : s->body2) genStmt(k.get());
        fc_->scopes.pop_back();
        if (!b().GetInsertBlock()->getTerminator()) {
            b().CreateCall(cdone, {});  // rethrows unless Fs->catch_except
            b().CreateBr(endBB);
        }
        b().SetInsertPoint(endBB);
    }

    // ---------------------------------------------------------- declarations
    // TempleOS reg-var semantics (doc examples): 32-bit LOCALS live in
    // 64-bit registers and are not truncated on assignment
    //   I32 i4=0x80000000;  i4>>1  == 0x40000000
    //   I32 i5=-0x80000000; i5>>1  == 0xFFFFFFFFC0000000
    // while 8/16-bit assignment truncates (i1=0x12345678 -> 0x5678).
    static TypePtr regSlotType(const TypePtr& t) {
        if (t->isInt() && t->intSize == 4) return tyInt(8, t->isUnsigned);
        return t;
    }

    void genVarDecl(const Stmt* s) {
        for (const VarDeclarator& d : s->decls) {
            if (s->isStatic) {
                genStaticLocal(d);
                continue;
            }
            TypePtr vt = regSlotType(d.type);
            AllocaInst* a = entryAlloca(memTy(vt), d.name);
            fc_->scopes.back()[d.name] = {a, vt};
            if (d.init) genInit(a, vt, d.init.get());
        }
    }

    void genStaticLocal(const VarDeclarator& d) {
        Constant* init = Constant::getNullValue(memTy(d.type));
        if (d.init) {
            Constant* c = tryConstInit(d.init.get(), d.type);
            if (c)
                init = c;
            else
                error(d.loc, "static var initializer must be a compile-time constant");
        }
        auto* g = new GlobalVariable(*mod_, memTy(d.type), false, GlobalValue::InternalLinkage,
                                     init, fc_->fn->getName() + "." + d.name);
        g->setAlignment(Align(8));
        fc_->scopes.back()[d.name] = {g, d.type};
    }

    void genInit(Value* addr, const TypePtr& t, const Expr* init) {
        if (init->kind == Ex::InitList) {
            // zero-fill then store element-wise
            b().CreateMemSet(addr, ConstantInt::get(llvm::Type::getInt8Ty(*ctx_), 0),
                             (uint64_t)t->size(), MaybeAlign(1));
            genInitList(addr, t, init);
            return;
        }
        if (t->kind == Type::Array && t->elem->isInt() && t->elem->intSize == 1 &&
            init->kind == Ex::StrLit) {
            // U8 buf[N] = "str";
            GlobalVariable* g = stringConst(init->strVal);
            uint64_t n = std::min<uint64_t>(init->strVal.size() + 1, (uint64_t)t->size());
            b().CreateMemCpy(addr, MaybeAlign(1), g, MaybeAlign(1), n);
            return;
        }
        if (isAggregate(t)) {
            RV rv = genExpr(init);
            // aggregate = pointer-to-aggregate copy
            b().CreateMemCpy(addr, MaybeAlign(1), asPtr(rv.v), MaybeAlign(1), (uint64_t)t->size());
            return;
        }
        RV rv = genExpr(init);
        storeTo(addr, rv, t);
    }

    void genInitList(Value* addr, const TypePtr& t, const Expr* lst) {
        if (t->kind == Type::Array) {
            int64_t esz = t->elem->size();
            for (size_t i = 0; i < lst->kids.size(); i++) {
                if ((int64_t)i >= t->arrayLen) {
                    error(lst->loc, "too many initializers");
                    break;
                }
                Value* ea =
                    b().CreateGEP(llvm::Type::getInt8Ty(*ctx_), addr, cI64((int64_t)i * esz));
                const Expr* k = lst->kids[i].get();
                if (k->kind == Ex::InitList)
                    genInitList(ea, t->elem, k);
                else
                    genInit(ea, t->elem, k);
            }
            return;
        }
        if (t->kind == Type::Class && t->cls) {
            // flatten inherited members first (base precedes derived)
            std::vector<const ClassMember*> flat;
            std::function<void(const ClassInfo*)> collect = [&](const ClassInfo* c) {
                if (c->base && c->base->cls) collect(c->base->cls.get());
                for (auto& m : c->members) flat.push_back(&m);
            };
            collect(t->cls.get());
            for (size_t i = 0; i < lst->kids.size(); i++) {
                if (i >= flat.size()) {
                    error(lst->loc, "too many initializers");
                    break;
                }
                Value* ea =
                    b().CreateGEP(llvm::Type::getInt8Ty(*ctx_), addr, cI64(flat[i]->offset));
                const Expr* k = lst->kids[i].get();
                if (k->kind == Ex::InitList)
                    genInitList(ea, flat[i]->type, k);
                else
                    genInit(ea, flat[i]->type, k);
            }
            return;
        }
        error(lst->loc, "initializer list for non-aggregate");
    }

    // constant initializers for globals/statics
    Constant* tryConstInit(const Expr* e, const TypePtr& t) {
        if (t->isF64()) {
            double d;
            if (evalConstF64(e, &d)) return ConstantFP::get(f64_, d);
            return nullptr;
        }
        if (t->isInt() || t->isPtr() || t->kind == Type::Func) {
            if (e->kind == Ex::StrLit)
                return ConstantExpr::getPtrToInt(stringConst(e->strVal), memTy(t));
            int64_t v;
            if (!evalConst(e, &v)) return nullptr;
            llvm::Type* mt = memTy(t);
            return ConstantInt::get(mt, (uint64_t)v, true);
        }
        if (t->kind == Type::Array && t->elem->isInt() && t->elem->intSize == 1 &&
            e->kind == Ex::StrLit) {
            std::string s = e->strVal;
            s.resize((size_t)t->size(), '\0');
            return ConstantDataArray::getString(*ctx_, s, false);
        }
        if (isAggregate(t) && e->kind == Ex::InitList) {
            // serialize constant bytes
            std::vector<uint8_t> bytes((size_t)t->size(), 0);
            if (!constBytes(e, t, bytes, 0)) return nullptr;
            return ConstantDataArray::get(*ctx_, ArrayRef<uint8_t>(bytes));
        }
        return nullptr;
    }

    bool constBytes(const Expr* e, const TypePtr& t, std::vector<uint8_t>& out, int64_t off) {
        if (e->kind != Ex::InitList) {
            if (t->isF64()) {
                double d;
                if (!evalConstF64(e, &d)) return false;
                memcpy(&out[(size_t)off], &d, 8);
                return true;
            }
            int64_t v;
            if (!evalConst(e, &v)) return false;
            memcpy(&out[(size_t)off], &v, (size_t)std::min<int64_t>(t->size(), 8));
            return true;
        }
        if (t->kind == Type::Array) {
            int64_t esz = t->elem->size();
            if ((int64_t)e->kids.size() > t->arrayLen) return false;
            for (size_t i = 0; i < e->kids.size(); i++)
                if (!constBytes(e->kids[i].get(), t->elem, out, off + (int64_t)i * esz))
                    return false;
            return true;
        }
        if (t->kind == Type::Class && t->cls) {
            std::vector<const ClassMember*> flat;
            std::function<void(const ClassInfo*)> collect = [&](const ClassInfo* c) {
                if (c->base && c->base->cls) collect(c->base->cls.get());
                for (auto& m : c->members) flat.push_back(&m);
            };
            collect(t->cls.get());
            if (e->kids.size() > flat.size()) return false;
            for (size_t i = 0; i < e->kids.size(); i++)
                if (!constBytes(e->kids[i].get(), flat[i]->type, out, off + flat[i]->offset))
                    return false;
            return true;
        }
        return false;
    }

    // ---------------------------------------------------------- top level
    void declareGlobal(GlobalVar& g) {
        std::string sym = g.name;
        bool isDecl = false;
        GlobalValue::LinkageTypes lk = GlobalValue::ExternalLinkage;
        switch (g.linkage) {
            case Linkage::ExternAlias:
            case Linkage::ImportAlias:
                sym = g.aliasSym;
                isDecl = true;
                break;
            case Linkage::Extern:
            case Linkage::Import:
                isDecl = true;
                break;
            case Linkage::Static:
                lk = GlobalValue::InternalLinkage;
                break;
            default:
                break;
        }
        llvm::Type* mt = memTy(g.type);
        auto* gv = new GlobalVariable(*mod_, mt, false, lk,
                                      isDecl ? nullptr : Constant::getNullValue(mt), sym);
        if (isDecl) gv->setLinkage(GlobalValue::ExternalLinkage);
        gv->setAlignment(Align(8));
        globals_[g.name] = {gv, g.type};
        if (!isDecl && g.init) {
            Constant* c = tryConstInit(g.init.get(), g.type);
            if (c) {
                gv->setInitializer(c);
                g.init.reset();  // done; no dynamic init needed
            }
        }
    }

    // ---------------------------------------------------------- functions
    void beginFn(FnCtx& fx, Function* fn, const FuncDecl* decl) {
        fx.fn = fn;
        fx.decl = decl;
        fx.b = std::make_unique<IRBuilder<>>(*ctx_);
        BasicBlock* entry = BasicBlock::Create(*ctx_, "entry", fn);
        fx.b->SetInsertPoint(entry);
        // alloca insertion marker
        fx.allocaMarker = fx.b->CreateAlloca(i32_, nullptr, "alloca.marker");
        fx.scopes.emplace_back();
        if (decl) {
            const TypePtr& rt = decl->type->ret;
            fx.retIsF64 = rt->kind == Type::Void ? 2 : (rt->isF64() ? 1 : 0);
            fx.isVariadic = decl->type->variadic;
        } else {
            fx.retIsF64 = 2;
        }
    }

    void genFuncBody(const FuncDecl& fd, Function* fn) {
        auto fx = std::make_unique<FnCtx>();
        FnCtx* prev = fc_;
        fc_ = fx.get();
        beginFn(*fx, fn, &fd);

        const auto& params = *fd.type->params;
        unsigned ai = 0;
        for (auto& p : params) {
            Argument* arg = fn->getArg(ai++);
            TypePtr pt = regSlotType(p.type ? p.type : tyI64());
            std::string nm = p.name.empty() ? "arg" + std::to_string(ai) : p.name;
            arg->setName(nm);
            AllocaInst* a = entryAlloca(memTy(pt), nm + ".addr");
            RV rv{arg, pt->isF64() ? tyF64() : tyI64()};
            storeTo(a, rv, pt);
            if (!p.name.empty()) fc_->scopes.back()[p.name] = {a, pt};
        }
        if (fd.type->variadic) {
            // 'I64 argc' and 'I64 argv[]' builtins
            Argument* argcA = fn->getArg(ai++);
            Argument* argvA = fn->getArg(ai++);
            argcA->setName("argc");
            argvA->setName("argv");
            AllocaInst* ac = entryAlloca(i64_, "argc.addr");
            AllocaInst* av = entryAlloca(i64_, "argv.addr");
            b().CreateStore(argcA, ac);
            b().CreateStore(argvA, av);
            fc_->scopes.back()["argc"] = {ac, tyI64()};
            fc_->scopes.back()["argv"] = {av, tyPtr(tyI64())};
        }

        for (auto& s : fd.body) genStmt(s.get());

        finishFn(*fx);
        fc_ = prev;
    }

    void finishFn(FnCtx& fx) {
        IRBuilder<>& bb = *fx.b;
        if (!bb.GetInsertBlock()->getTerminator()) {
            if (fx.retIsF64 == 2)
                bb.CreateRetVoid();
            else if (fx.retIsF64 == 1)
                bb.CreateRet(ConstantFP::get(f64_, 0.0));
            else
                bb.CreateRet(cI64(0));
        }
        for (auto& l : fx.labels) {
            if (!fx.labelsDefined.count(l.first)) {
                fprintf(stderr, "error: goto to undefined label '%s' in %s\n", l.first.c_str(),
                        fx.fn->getName().str().c_str());
                hadError_ = true;
                // make it valid IR anyway
                if (!l.second->getTerminator()) {
                    IRBuilder<> db(l.second);
                    if (fx.retIsF64 == 2)
                        db.CreateRetVoid();
                    else if (fx.retIsF64 == 1)
                        db.CreateRet(ConstantFP::get(f64_, 0.0));
                    else
                        db.CreateRet(cI64(0));
                }
            }
        }
        fx.allocaMarker->eraseFromParent();
    }
};

CodegenResult CG::run() {
    // pre-pass: declare all functions & globals so ordering doesn't matter
    for (auto& item : prog_.items) {
        if (item.func) {
            FuncDecl& fd = *item.func;
            Function* f = declareFunc(fd);
            declToFn_[&fd] = f;
            // bind the first occurrence; later dups rebind during the walk
            if (!funcs_.count(fd.name)) funcs_[fd.name] = {f, fd.type, &fd};
        } else if (item.global) {
            if (!globals_.count(item.global->name))
                declareGlobal(*item.global);
            else if (item.global->linkage == Linkage::Normal)
                error(item.global->loc, "duplicate global '" + item.global->name + "'");
        }
    }

    // startup function collects top-level statements in order
    Function* startFn = Function::Create(FunctionType::get(voidTy_, {}, false),
                                         GlobalValue::ExternalLinkage, "__HC_startup", mod_.get());
    startup_ = std::make_unique<FnCtx>();
    beginFn(*startup_, startFn, nullptr);

    for (auto& item : prog_.items) {
        if (item.func) {
            FuncDecl& fd = *item.func;
            // rebind at this position: "older syms ... will be overshadowed"
            funcs_[fd.name] = {declToFn_[&fd], fd.type, &fd};
            if (fd.hasBody) genFuncBody(fd, declToFn_[&fd]);
        } else if (item.global) {
            if (item.global->init) {  // dynamic init runs at its position
                fc_ = startup_.get();
                VarSym& vs = globals_[item.global->name];
                genInit(vs.addr, vs.type, item.global->init.get());
            }
        } else if (item.stmt) {
            fc_ = startup_.get();
            genStmt(item.stmt.get());
        }
    }

    fc_ = startup_.get();
    finishFn(*startup_);

    if (aot_) {
        // int main(int argc, char **argv)
        Function* mainFn = Function::Create(FunctionType::get(i32_, {i32_, ptr_}, false),
                                            GlobalValue::ExternalLinkage, "main", mod_.get());
        BasicBlock* bb = BasicBlock::Create(*ctx_, "entry", mainFn);
        IRBuilder<> mb(bb);
        Function* rtInit = mod_->getFunction("HC_RtInit");
        if (!rtInit)
            rtInit = Function::Create(FunctionType::get(voidTy_, {i64_, ptr_}, false),
                                      GlobalValue::ExternalLinkage, "HC_RtInit", mod_.get());
        mb.CreateCall(rtInit, {mb.CreateSExt(mainFn->getArg(0), i64_), mainFn->getArg(1)});
        mb.CreateCall(startFn, {});
        mb.CreateRet(ConstantInt::get(i32_, 0));
    }

    std::string verr;
    raw_string_ostream os(verr);
    if (verifyModule(*mod_, &os)) {
        fprintf(stderr, "internal error: invalid IR generated:\n%s\n", os.str().c_str());
        hadError_ = true;
    }

    CodegenResult r;
    r.ctx = std::move(ctx_);
    r.module = std::move(mod_);
    r.ok = !hadError_;
    return r;
}

}  // namespace

CodegenResult codegen(Program& prog, const std::string& moduleName, bool aotMode) {
    CG cg(prog, moduleName, aotMode);
    return cg.run();
}

}  // namespace hc
