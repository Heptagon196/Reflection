#include "TypeID.h"
#include <functional>
#include <map>

bool TypeID::canImplicitlyConvertTo(const TypeID& other) const {
    int cnt = 0;
    for (const auto& type : TypeID::implicitConvertList) {
        if (type == this->hash) {
            cnt++;
        }
        if (type == other.hash) {
            cnt++;
        }
    }
    return cnt == 2;
}

bool TypeID::checkRefAndConst(const TypeID& other) const {
    return other.is_const || (other.is_ref && !is_const) || is_ref || !other.is_ref;
}

bool TypeID::isNull() const {
    return hash == 0;
}

size_t TypeID::getHash() const {
    return hash;
}

std::string_view TypeID::getName() const {
    return name;
}

bool TypeID::canBeAppliedTo(const TypeID& other) const {
    return (checkRefAndConst(other) && (hash == other.hash || canImplicitlyConvertTo(other)));
}

bool TypeID::operator == (const TypeID& other) const {
    return hash == other.hash && is_ref == other.is_ref && is_const == other.is_const;
}

bool TypeID::operator != (const TypeID& other) const {
    return !(*this == other);
}

bool TypeID::operator < (const TypeID& other) const {
    return hash < other.hash;
}

#define DEFLIST(DEF)                                                    \
    DEF(int8_t), DEF(int16_t), DEF(int32_t), DEF(int64_t),              \
    DEF(uint8_t), DEF(uint16_t), DEF(uint32_t), DEF(uint64_t),          \
    DEF(float), DEF(double), DEF(long double)

#define DEFLIST2(orig, DEF)                                                             \
    DEF(orig, int8_t), DEF(orig, int16_t), DEF(orig, int32_t), DEF(orig, int64_t),      \
    DEF(orig, uint8_t), DEF(orig, uint16_t), DEF(orig, uint32_t), DEF(orig, uint64_t),  \
    DEF(orig, float), DEF(orig, double), DEF(orig, long double)

#define DEF(t) TypeID::get<t>().getHash()

const std::vector<size_t> TypeID::implicitConvertList = {
    DEFLIST(DEF)
};

#undef DEF

#define DEF_SINGLE(orig, target) \
    { TypeID::get<target>().getHash(), std::function([](void* instance) -> std::shared_ptr<void> { return std::make_shared<target>(*(orig*)instance); }) }

#define DEF(type) \
    { TypeID::get<type>().getHash(), { DEFLIST2(type, DEF_SINGLE) }}

static const std::unordered_map<size_t, std::unordered_map<size_t, std::function<std::shared_ptr<void>(void*)>>> convertFunc = {
    DEFLIST(DEF)
};

#undef DEF_SINGLE
#undef DEF

std::shared_ptr<void> TypeID::implicitConvertInstance(void* instance, TypeID target) {
    return (convertFunc.at(getHash()).at(target.getHash()))(instance);
}

size_t std::hash<TypeID>::operator() (const TypeID& id) const {
    return id.hash;
}
