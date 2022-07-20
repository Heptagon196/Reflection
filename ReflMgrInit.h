#pragma once
#include <sstream>
#include "ReflMgr.h"
namespace ReflMgrTool {
    void InitBaseTypes();
    void InitModules();
    void Init();
    template<typename T> void AutoRegister() {
        auto& mgr = ReflMgr::Instance();
#define DEFOP_BASE(name, func)                                                          \
        if constexpr (MetaMethods::has_operator_ ## name<T, T>) {                       \
            mgr.AddMethod<T>(std::function(func), MetaMethods::operator_ ## name);      \
        }                                                                               \

#define DEFBIOP(name, op) \
        DEFOP_BASE(name, [](T* self, const T& other) -> decltype(auto) { return (*self) op other; })
#define DEFSINGLE(name, func)                                                   \
        if constexpr (MetaMethods::has_operator_ ## name<T>) {                  \
            mgr.AddMethod<T>(std::function(                                     \
                [](T* self) -> decltype(auto) { return func; }                  \
            ), MetaMethods::operator_ ## name);                                 \
        }                                                                       \

        DEFBIOP(add, +);
        DEFBIOP(sub, -);
        DEFBIOP(mul, *);
        DEFBIOP(div, /);
        DEFBIOP(mod, %);
        DEFOP_BASE(pow, [](T* self, T other) -> double { return std::pow((double)*self, (double)other); });
        DEFBIOP(eq, ==);
        DEFBIOP(ne, !=);
        DEFBIOP(lt, <);
        DEFBIOP(le, <=);
        DEFBIOP(gt, >);
        DEFBIOP(ge, >=);
        DEFSINGLE(unm, -(*self));
        DEFSINGLE(indirection, *(*self));
        DEFSINGLE(begin, std::begin(*self));
        DEFSINGLE(end, std::end(*self));
        if constexpr (MetaMethods::has_operator_index<T, int>) {
            mgr.AddMethod<T>(std::function([](T* self, int idx) -> decltype(auto) { return (*self)[idx]; }), MetaMethods::operator_index);
        }
        if constexpr (MetaMethods::has_operator_inc_pre<T>) {
            mgr.AddMethod<T>(std::function([](T* self) { return ++(*self); }), MetaMethods::operator_inc);
        }
        if constexpr (MetaMethods::has_operator_dec_pre<T>) {
            mgr.AddMethod<T>(std::function([](T* self) { return --(*self); }), MetaMethods::operator_dec);
        }
        if constexpr (MetaMethods::has_operator_inc_post<T>) {
            mgr.AddMethod<T>(std::function([](T* self, int) { return (*self)++; }), MetaMethods::operator_inc);
        }
        if constexpr (MetaMethods::has_operator_dec_post<T>) {
            mgr.AddMethod<T>(std::function([](T* self, int) { return (*self)--; }), MetaMethods::operator_dec);
        }
        if constexpr (MetaMethods::has_operator_tostring<T>) {
            mgr.AddMethod<T>(
                std::function<std::string(T*)>([](T* self) {
                    std::stringstream ss;
                    ss << *self;
                    return (std::string)(ss.str());
                }),
                MetaMethods::operator_tostring
            );
        }
        // index, call, ctor, dtor, copy, parse

#undef DEFBIOP
#undef DEFBIOP_BASE
#undef DEFSINGLE
#undef DEFOP_BASE
    }
}
