#include "ast.hpp"

namespace hc {

const ClassMember* ClassInfo::findMember(const std::string& n) const {
    for (auto& m : members)
        if (m.name == n) return &m;
    if (base && base->cls) return base->cls->findMember(n);
    return nullptr;
}

int64_t Type::size() const {
    switch (kind) {
        case Void:
            return 0;
        case Int:
            return intSize;
        case F64:
            return 8;
        case Ptr:
            return 8;
        case Func:
            return 8;
        case Array:
            return elem->size() * arrayLen;
        case Class:
            return cls ? cls->size : 0;
    }
    return 0;
}

std::string Type::str() const {
    switch (kind) {
        case Void:
            return "U0";
        case Int: {
            std::string s = isUnsigned ? "U" : "I";
            return s + std::to_string(intSize * 8);
        }
        case F64:
            return "F64";
        case Ptr:
            return elem->str() + "*";
        case Array:
            return elem->str() + "[" + std::to_string(arrayLen) + "]";
        case Class:
            return cls ? cls->name : "<class>";
        case Func:
            return "<fun>";
    }
    return "?";
}

static TypePtr mk(Type t) { return std::make_shared<Type>(std::move(t)); }

TypePtr tyVoid() {
    static TypePtr t = mk(Type(Type::Void));
    return t;
}
TypePtr tyInt(int size, bool uns) {
    Type t(Type::Int);
    t.intSize = size;
    t.isUnsigned = uns;
    return mk(t);
}
TypePtr tyI64() {
    static TypePtr t = tyInt(8, false);
    return t;
}
TypePtr tyU64() {
    static TypePtr t = tyInt(8, true);
    return t;
}
TypePtr tyU8() {
    static TypePtr t = tyInt(1, true);
    return t;
}
TypePtr tyF64() {
    static TypePtr t = mk(Type(Type::F64));
    return t;
}
TypePtr tyPtr(TypePtr elem) {
    Type t(Type::Ptr);
    t.elem = std::move(elem);
    return mk(t);
}
TypePtr tyArray(TypePtr elem, int64_t n) {
    Type t(Type::Array);
    t.elem = std::move(elem);
    t.arrayLen = n;
    return mk(t);
}

}  // namespace hc
