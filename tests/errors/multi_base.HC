//ERR: only one base class is allowed
class A { I64 x; };
class B { I64 y; };
class C:A,B { I64 z; };
