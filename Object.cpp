#include "ReflMgr.h"
#include "Object.h"

TemplatedTypeID::TemplatedTypeID() {
    base = TypeID::get<void>();
}

TemplatedTypeID::TemplatedTypeID(TypeID type) {
    base = type;
}

std::string TemplatedTypeID::getName() const {
    if (args.size() == 0) {
        return (std::string)base.getName();
    }
    auto ret = (std::string)base.getName() + "<";
    for (int i = 0; i < args.size(); i++) {
        ret += args[i].getName() + ", ";
    }
    ret.pop_back();
    ret[ret.size() - 1] = '>';
    return ret;
}

ObjectInfo::ObjectInfo(TypeID type) {
    this->id.base = type;
}

ObjectInfo::ObjectInfo(TemplatedTypeID type) {
    this->id = type;
}

Namespace::Namespace(std::string_view space) : space(TypeID::getRaw(space)) {
    vObj = SharedObject{ this->space, nullptr };
}

TypeID Namespace::Type() const {
    return space;
}

Namespace Namespace::getNamespace(std::string_view subSpace) const {
    return { (std::string)space.getName() + "::" + (std::string)subSpace };
}

TypeID Namespace::getClass(std::string_view clsName) const {
    return TypeID::getRaw((std::string)space.getName() + (std::string)clsName);
}

SharedObject Namespace::Invoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(vObj, method, params);
}

const Namespace Namespace::Global = { "" };

const SharedObject SharedObject::Null = { TypeID::get<void>(), nullptr, false };

const ObjectPtr ObjectPtr::Null = { std::make_shared<ObjectInfo>(TypeID::get<void>()), nullptr };

ObjectPtr::ObjectPtr() : info(std::make_shared<ObjectInfo>(TypeID::get<void>())), content(nullptr) {}
ObjectPtr::ObjectPtr(const ObjectPtr& other) : info(other.info), content(other.content) {}
ObjectPtr::ObjectPtr(std::shared_ptr<ObjectInfo> info, void* ptr) : info(info), content(ptr) {}
ObjectPtr::ObjectPtr(const SharedObject& obj) : info(obj.info), content(obj.GetRawPtr()) {}
ObjectPtr::ObjectPtr(TemplatedTypeID id, void* ptr) : info(std::make_shared<ObjectInfo>(id)), content(ptr) {}

SharedObject ObjectPtr::ToSharedObj() const {
    return ToShared<ObjectPtr>()(*this);
}

TemplatedTypeID& ObjectPtr::GetType() const {
    return info->id;
}

void* ObjectPtr::GetPtr() const {
    return content;
}

void* ObjectPtr::GetRawPtr() const {
    return content;
}

ObjectPtr ObjectPtr::GetField(std::string_view name) const {
    return ReflMgr::Instance().GetField(*this, name);
}

SharedObject ObjectPtr::Invoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(*this, method, params);
}

SharedObject ObjectPtr::TryInvoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(*this, method, params, {}, false);
}

SharedObject::SharedObject() : info(std::make_shared<ObjectInfo>(TypeID::get<void>())), content(nullptr), fallbackPtr(nullptr) {};
SharedObject::SharedObject(const SharedObject& other) : info(other.info), content(other.content), fallbackPtr(other.fallbackPtr) {}
SharedObject::SharedObject(TypeID type, void* objPtr) : info(std::make_shared<ObjectInfo>(type)), fallbackPtr(objPtr) {};
SharedObject::SharedObject(std::shared_ptr<ObjectInfo> info, void* objPtr) : info(info), fallbackPtr(objPtr) {}
SharedObject::SharedObject(const TemplatedTypeID& id, std::shared_ptr<void> ptr, bool call_ctor) : info(std::make_shared<ObjectInfo>(id)), content(ptr), fallbackPtr(nullptr) {
    if (id.base == TypeID::get<void>()) {
        return;
    }
    if (call_ctor) {
        ctor({});
    }
}

SharedObject& SharedObject::AddTemplateArgs(const ArgsTypeList& templateArgs) {
    if (templateArgs.size() > 0) {
        info->id.args = templateArgs;
    }
    return *this;
}

TemplatedTypeID& SharedObject::GetType() const {
    return info->id;
}

std::shared_ptr<void> SharedObject::GetPtr() const {
    return content;
}

bool SharedObject::isObjectPtr() const {
    return fallbackPtr != nullptr;
}

void* SharedObject::GetRawPtr() const {
    if (isObjectPtr()) {
        return fallbackPtr;
    }
    return content.get();
}

ObjectPtr SharedObject::GetField(std::string_view name) const {
    return ReflMgr::Instance().GetField(*this, name);
}

SharedObject SharedObject::Invoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(*this, method, params);
}

SharedObject SharedObject::TryInvoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(*this, method, params, {}, false);
}

ObjectPtr SharedObject::ToObjectPtr() const {
    if (isObjectPtr()) {
        return ObjectPtr{ info, fallbackPtr };
    }
    return ObjectPtr{ info, content.get() };
}

#define DEFOPS(T)               \
    BIDEF(T, +, add)            \
    BIDEF(T, -, sub)            \
    BIDEF(T, *, mul)            \
    BIDEF(T, /, div)            \
    BIDEF(T, %, mod)            \
    BIDEF(T, ^, pow)            \
    BIDEF(T, ==, eq)            \
    BIDEF(T, !=, ne)            \
    BIDEF(T, <, lt)             \
    BIDEF(T, <=, le)            \
    BIDEF(T, >, gt)             \
    BIDEF(T, >=, ge)            \
    SharedObject T::operator - () const {                                                               \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_unm, {});                    \
    }                                                                                                   \
    SharedObject T::operator [] (ObjectPtr idx) const {                                                 \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_index, { idx });             \
    }                                                                                                   \
    SharedObject T::operator () (const std::vector<ObjectPtr>& args) const {                            \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_call, args);                 \
    }                                                                                                   \
    SharedObject T::tostring() const {                                                                  \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_tostring, {});               \
    }                                                                                                   \
    void T::ctor(const std::vector<ObjectPtr>& args) const {                                            \
        ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_ctor, args, {}, false);             \
    }                                                                                                   \
    void T::dtor() const {                                                                              \
        ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_dtor, {}, {}, false);               \
    }                                                                                                   \
    SharedObject T::begin() {                                                                           \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_begin, {});                  \
    }                                                                                                   \
    SharedObject T::end() {                                                                             \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_end, {});                    \
    }                                                                                                   \
    SharedObject T::operator ++ () {                                                                    \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_inc, {});                    \
    }                                                                                                   \
    SharedObject T::operator -- () {                                                                    \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_dec, {});                    \
    }                                                                                                   \
    SharedObject T::operator ++ (int) {                                                                 \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_inc,                         \
            { SharedObject::New<int>() }                                                                \
        );                                                                                              \
    }                                                                                                   \
    SharedObject T::operator -- (int) {                                                                 \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_dec,                         \
            { SharedObject::New<int>() }                                                                \
        );                                                                                              \
    }                                                                                                   \
    SharedObject T::operator * () {                                                                     \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_indirection, {});            \
    }                                                                                                   \

#define BIDEF(T, op, meta)                                                                                      \
    SharedObject T::operator op (const T& other) const {                                                        \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_ ## meta, std::vector<T>{ other });  \
    }

DEFOPS(SharedObject)
DEFOPS(ObjectPtr)

#undef BIDEF

std::ostream& operator << (std::ostream& out, const ObjectPtr& ptr) {
    auto obj = ptr.Invoke(MetaMethods::operator_tostring, {});
    if (obj.GetType().base != TypeID::get<std::string>()) {
        return out;
    }
    out << obj.Get<std::string>();
    return out;
}

std::ostream& operator << (std::ostream& out, const SharedObject& ptr) {
    auto obj = ptr.Invoke(MetaMethods::operator_tostring, {});
    if (obj.GetType().base != TypeID::get<std::string>()) {
        return out;
    }
    out << obj.Get<std::string>();
    return out;
}

SharedObject::~SharedObject() {
    if (info->id.base == TypeID::get<void>() || content.use_count() != 1) {
        return;
    }
    dtor();
}

ObjectPtr::operator bool() const {
    return this->Get<bool>();
}

SharedObject::operator bool() const {
    return this->Get<bool>();
}
