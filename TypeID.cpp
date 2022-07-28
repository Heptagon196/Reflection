#include "TypeID.h"
#include <functional>
#include <map>

#define DEFLIST(DEF)                                                    \
    DEF(int8_t), DEF(int16_t), DEF(int32_t), DEF(int64_t),              \
    DEF(uint8_t), DEF(uint16_t), DEF(uint32_t), DEF(uint64_t),          \
    DEF(float), DEF(double), DEF(long double)

#define DEFLIST2(orig, DEF)                                                             \
    DEF(orig, int8_t), DEF(orig, int16_t), DEF(orig, int32_t), DEF(orig, int64_t),      \
    DEF(orig, uint8_t), DEF(orig, uint16_t), DEF(orig, uint32_t), DEF(orig, uint64_t),  \
    DEF(orig, float), DEF(orig, double), DEF(orig, long double)

#define DEF(t) TypeID::get<t>()

const std::vector<TypeID> TypeID::implicitConvertList = {
    DEFLIST(DEF)
};

#undef DEF

#define DEF_SINGLE(orig, target) \
    { TypeID::get<target>(), std::function([](void* instance) -> std::shared_ptr<void> { return std::make_shared<target>(*(orig*)instance); }) }

#define DEF(type) \
    { TypeID::get<type>(), { DEFLIST2(type, DEF_SINGLE) }}

static const std::map<TypeID, std::map<TypeID, std::function<std::shared_ptr<void>(void*)>>> convertFunc = {
    DEFLIST(DEF)
};

#undef DEF_SINGLE
#undef DEF

std::shared_ptr<void> TypeID::implicitConvertInstance(void* instance, TypeID target) {
    return (convertFunc.at(*this).at(target))(instance);
}
