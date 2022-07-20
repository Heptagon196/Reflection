#pragma once

#include <cmath>
#include <string_view>
#include <type_traits>
#include <iostream>

namespace MetaMethods {
#define DEFOP(op) static constexpr std::string_view operator_ ## op = "__" #op;
#define DEFBIOP(name, op) DEFDOUBLE(name, a op b)
#define DEFSINGLE(name, func) DEFOP(name); template<typename T> concept has_operator_ ## name = requires(T a) { func; }
#define DEFDOUBLE(name, func) DEFOP(name); template<typename T, typename U> concept has_operator_ ## name = requires(T a, U b) { func; }
#define DEFMULTI(name, func) DEFOP(name); template<typename T, typename... Args> concept has_operator_ ## name = requires(T a, Args... args) { func; }
    DEFBIOP(add, +);
    DEFDOUBLE(sub, a - b; { a - b } -> std::same_as<T>);
    DEFBIOP(mul, *);
    DEFBIOP(div, /);
    DEFBIOP(mod, %);
    DEFDOUBLE(pow, (T)(double)a; (U)(double)b);
    DEFBIOP(eq, ==);
    DEFBIOP(ne, !=);
    DEFBIOP(lt, <);
    DEFBIOP(le, <=);
    DEFBIOP(gt, >);
    DEFBIOP(ge, >=);
    DEFSINGLE(unm, -a);
    DEFDOUBLE(index, a[b]);
    DEFMULTI(call, a(args...));
    DEFMULTI(ctor, a.ctor(args...));
    DEFSINGLE(dtor, a.dtor());
    DEFSINGLE(begin, std::begin(a));
    DEFSINGLE(end, std::end(a));
    DEFSINGLE(indirection, *a);
    DEFOP(inc);
    template<typename T> concept has_operator_inc_pre = requires(T a) { ++a; };
    template<typename T> concept has_operator_inc_post = requires(T a) { a++; };
    DEFOP(dec);
    template<typename T> concept has_operator_dec_pre = requires(T a) { --a; };
    template<typename T> concept has_operator_dec_post = requires(T a) { a--; };
    DEFOP(copy); template<typename T> concept has_operator_copy = requires(T a, T b) { a = b.copy(); };
    DEFOP(tostring); template<typename T> concept has_operator_tostring = requires(T a, std::ostream b) { b << a; };
#undef DEFMULTI
#undef DEFDOUBLE
#undef DEFSINGLE
#undef DEFBIOP
#undef DEFOP
};
