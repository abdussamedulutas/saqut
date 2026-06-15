// Faz 0 — Type birim testleri (çerçevesiz; assert + çıktı).
// Koşmak için: tests/run.sh  (veya g++ -std=c++20 -Wall -Wextra -Isrc ...)
#include "core/type.hpp"
#include <iostream>
#include <cassert>

int main() {
    Type i = Type::Int();
    Type i2 = Type::Int();
    Type f = Type::Float();
    Type arrI = Type::array(Type::Int());
    Type arrI2 = Type::array(Type::Int());
    Type arrF = Type::array(Type::Float());
    Type fn = Type::function(Type::Int(), {Type::Int(), Type::Int()});
    Type st = Type::structType("Point");
    Type err = Type::error();

    // equals — yapısal, katı (gizli dönüşüm yok)
    assert(i.equals(i2));
    assert(!i.equals(f));
    assert(arrI.equals(arrI2));
    assert(!arrI.equals(arrF));
    assert(!arrI.equals(i));
    assert(fn.equals(Type::function(Type::Int(), {Type::Int(), Type::Int()})));
    assert(!fn.equals(Type::function(Type::Int(), {Type::Int()})));
    assert(st.equals(Type::structType("Point")));
    assert(!st.equals(Type::structType("Vec")));
    assert(err.equals(Type::error()));

    // yüklemler
    assert(i.isNumeric() && !st.isNumeric());
    assert(Type::Void().isVoid());
    assert(err.isError());

    // toString
    assert(i.toString() == "int");
    assert(arrI.toString() == "int[]");
    assert(fn.toString() == "fn(int,int)->int");
    assert(st.toString() == "struct Point");
    assert(err.toString() == "<error>");

    // fromName
    assert(Type::fromName("int").equals(Type::Int()));
    assert(Type::fromName("bool").equals(Type::Bool()));
    assert(Type::fromName("bogus").isError());

    std::cout << "test_type: TUM TESTLER GECTI\n";
    return 0;
}
