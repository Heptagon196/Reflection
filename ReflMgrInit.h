#pragma once
#include <sstream>
#include "ReflMgr.h"
namespace ReflMgrTool {
    void InitBaseTypes();
    void InitModules();
    void Init();
    template<typename T> void AutoRegister() {
        auto& mgr = ReflMgr::Instance();
        mgr.AddClass<T>();

#define METHODS_CONCEPT(name) MetaMethods::has_operator_ ## name
#define METHODS_NAME(name) MetaMethods::operator_ ## name

#define DEFBIOP_BASE(U, name, func)                                             \
        if constexpr (METHODS_CONCEPT(name)<T, U>) {                            \
            mgr.AddMethod<T>(std::function(                                     \
                [](T* self, const U& other) -> decltype(auto) { func }          \
            ), METHODS_NAME(name));                                             \
        }                                                                       \

#define DEFBIOP(name, op) \
        DEFBIOP_BASE(T, name, return (*self) op other;)

#define DEFSINGLE_BASE(name, func)                                              \
        if constexpr (METHODS_CONCEPT(name)<T>) {                               \
            mgr.AddMethod<T>(std::function(                                     \
                [](T* self) -> decltype(auto) { func }                          \
            ), METHODS_NAME(name));                                             \
        }                                                                       \

#define DEFSINGLE(name, func) \
        DEFSINGLE_BASE(name, return func;)

        DEFBIOP(add, +);
        DEFBIOP(sub, -);
        DEFBIOP(mul, *);
        DEFBIOP(div, /);
        DEFBIOP(mod, %);
        DEFBIOP_BASE(T, pow, return std::pow((double)*self, (double)other););
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
        DEFBIOP_BASE(int, index, return (*self)[other];);
        DEFBIOP_BASE(size_t, index, return (*self)[other];);
        DEFBIOP_BASE(T, ctor, self->ctor(other););
        DEFSINGLE_BASE(dtor, self->dtor(););
        if constexpr (MetaMethods::has_operator_inc_pre<T>) {
            mgr.AddMethod<T>(std::function([](T* self) -> decltype(auto) { return ++(*self); }), MetaMethods::operator_inc);
        }
        if constexpr (MetaMethods::has_operator_dec_pre<T>) {
            mgr.AddMethod<T>(std::function([](T* self) -> decltype(auto) { return --(*self); }), MetaMethods::operator_dec);
        }
        if constexpr (MetaMethods::has_operator_inc_post<T>) {
            mgr.AddMethod<T>(std::function([](T* self, int) -> decltype(auto) { return (*self)++; }), MetaMethods::operator_inc);
        }
        if constexpr (MetaMethods::has_operator_dec_post<T>) {
            mgr.AddMethod<T>(std::function([](T* self, int) -> decltype(auto) { return (*self)--; }), MetaMethods::operator_dec);
        }
        if constexpr (MetaMethods::has_operator_tostring<T>) {
            mgr.AddMethod<T>(
                std::function<std::string(T*)>([](T* self) -> decltype(auto) {
                    std::stringstream ss;
                    ss << *self;
                    return (std::string)(ss.str());
                }),
                MetaMethods::operator_tostring
            );
        }

#undef METHODS_CONCEPT
#undef METHODS_NAME
#define METHODS_CONCEPT(name) ContainerMethods::has_ ## name
#define METHODS_NAME(name) #name
#define VALUE typename T::value_type
#define KEY typename T::key_type

        if constexpr (ContainerMethods::has_value_type<T>) {
            DEFBIOP_BASE(VALUE, push_back, self->push_back(other););
            DEFBIOP_BASE(VALUE, push_front, self->push_back(other););
            DEFBIOP_BASE(VALUE, push, self->push_back(other););
        }
        if constexpr (ContainerMethods::has_key_type<T>) {
            DEFBIOP_BASE(KEY, find, return self->find(other););
            DEFBIOP_BASE(KEY, at, return self->find(other););
            if constexpr (MetaMethods::has_operator_index<T, KEY>) {
                mgr.AddMethod<T>(std::function([](T* self, const KEY& idx) -> decltype(auto) { return (*self)[idx]; }), MetaMethods::operator_index);
            }
        }
        DEFSINGLE(size, self->size());
        DEFSINGLE_BASE(pop_back, self->pop_back(););
        DEFSINGLE_BASE(pop_front, self->pop_front(););
        DEFSINGLE_BASE(pop, self->pop(););

#undef KEY
#undef VALUE
#undef METHODS_CONCEPT
#undef METHODS_NAME
#undef DEFBIOP
#undef DEFBIOP_BASE
#undef DEFSINGLE
#undef DEFSINGLE_BASE
    }
}
