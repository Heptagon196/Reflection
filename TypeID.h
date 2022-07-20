#pragma once
#include <iostream>
#include <string_view>
#include <functional>
#include "TemplateUtils.h"

class TypeID {
    private:
        size_t hash;
        std::string_view name;
        bool is_ref;
        bool is_const;
        static constexpr size_t calculateHash(std::string_view s) {
            size_t hash = sizeof(size_t) == 8 ? 0xcbf29ce484222325 : 0x811c9dc5;
            const size_t prime = sizeof(size_t) == 8 ? 0x00000100000001b3 : 0x01000193;
            for (char ch : s) {
                hash ^= (size_t)ch;
                hash *= prime;
            }
            return hash;
        }
        static constexpr std::string_view cutString(std::string_view base, int removeFront, int removeEnd) {
            return base.substr(removeFront, base.length() - removeFront - removeEnd);
        }
        template<typename T>
        static constexpr std::string_view TypeToString() {
#if defined(__clang__)
            return cutString(__PRETTY_FUNCTION__, 52, 1);
#elif defined(__GNUC__)
            return cutString(__PRETTY_FUNCTION__, 67, 50);
#elif defined(_MSC_VER)
            return cutString(__FUNCSIG__, 95, 7);
#endif
        }
    public:
        TypeID() : hash(0) {}
        constexpr TypeID(std::string_view showName, std::string_view trueName, bool is_ref, bool is_const) : hash(calculateHash(trueName)), name(showName), is_ref(is_ref), is_const(is_const) {}
        template<typename T>
        static constexpr TypeID get() {
            return TypeID(TypeToString<T>(), TypeToString<typename std::remove_const<typename std::remove_reference<T>::type>::type>(), std::is_reference<T>(), std::is_const<T>());
        }
        static constexpr TypeID getRaw(std::string_view name) {
            return TypeID(name, name, false, false);
        }
        size_t getHash() const {
            return hash;
        }
        std::string_view getName() const {
            return name;
        }
        bool canBeAppliedTo(const TypeID& other) const {
            return (*this == other) || ((hash == other.hash) && (other.is_const || (other.is_ref && !is_const) || is_ref));
        }
        bool operator == (const TypeID& other) const {
            return hash == other.hash && is_ref == other.is_ref && is_const == other.is_const;
        }
        bool operator != (const TypeID& other) const {
            return !(*this == other);
        }
        bool operator < (const TypeID& other) const {
            return hash < other.hash;
        }
};
