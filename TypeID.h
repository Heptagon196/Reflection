#pragma once
#include <iostream>
#include <functional>
#include <vector>
#include <string_view>
#include <memory>
#include "TemplateUtils.h"

class TypeID {
    private:
        friend std::hash<TypeID>;
        size_t hash;
        std::string_view name;
        bool is_ref;
        bool is_const;
        static const std::vector<size_t> implicitConvertList;
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
        bool canImplicitlyConvertTo(const TypeID& other) const;
        bool checkRefAndConst(const TypeID& other) const;
    public:
        TypeID() : hash(0) {}
        bool isNull() const;
        constexpr TypeID(std::string_view showName, std::string_view trueName, bool is_ref, bool is_const) : hash(calculateHash(trueName)), name(showName), is_ref(is_ref), is_const(is_const) {}
        template<typename T>
        static constexpr TypeID get() {
            return TypeID(TypeToString<T>(), TypeToString<typename std::remove_const<typename std::remove_reference<T>::type>::type>(), std::is_reference<T>(), std::is_const<T>());
        }
        static constexpr TypeID getRaw(std::string_view name) {
            return TypeID(name, name, false, false);
        }
        size_t getHash() const;
        std::string_view getName() const;
        bool canBeAppliedTo(const TypeID& other) const;
        std::shared_ptr<void> implicitConvertInstance(void* instance, TypeID target);
        bool operator == (const TypeID& other) const;
        bool operator != (const TypeID& other) const;
        bool operator < (const TypeID& other) const;
};

namespace std {
    template<> class hash<TypeID> {
        public:
            size_t operator () (const TypeID& id) const;
    };
};

struct TypeIDHashEqual {
    bool operator() (const TypeID& lhr, const TypeID& rhs) const {
        return lhr.getHash() == rhs.getHash();
    }
};

template<typename T>
using TypeIDMap = std::unordered_map<TypeID, T, std::hash<TypeID>, TypeIDHashEqual>;
