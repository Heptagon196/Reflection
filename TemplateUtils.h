#pragma once
#include <type_traits>
template<typename... Args> struct TypeList {};

template<typename... List>
struct Count {
    static constexpr int count = 0;
};

template<>
struct Count<TypeList<>> {
    static constexpr int count = 0;
};

template<typename T, typename... List>
struct Count<TypeList<T, List...>> {
    static constexpr int count = Count<TypeList<List...>>::count + 1;
};

template<typename... Args>
struct MethodTraits;

template<typename Ret, typename Class, typename... Args>
struct MethodTraits<Ret(Class::*)(Args...)> {
    using ReturnType = Ret;
    using ArgList = TypeList<Args...>;
};

template<typename Ret, typename U, typename... Args>
struct MethodType {
    using Type = Ret (U::*)(Args...);
};

template<typename Ret, typename... Args>
struct FunctionType {
    using Type = Ret (*)(Args...);
};
