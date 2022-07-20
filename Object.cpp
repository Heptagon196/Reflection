#include "ReflMgr.h"
#include "Object.h"

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

const SharedObject SharedObject::Null = { TypeID::get<void>(), nullptr };

const ObjectPtr ObjectPtr::Null = { TypeID::get<void>(), nullptr };

ObjectPtr::ObjectPtr() : id(TypeID::get<void>()), ptr(nullptr) {}
ObjectPtr::ObjectPtr(const ObjectPtr& other) : id(other.id), ptr(other.ptr) {}
ObjectPtr::ObjectPtr(TypeID id, void* ptr) : id(id), ptr(ptr) {}
ObjectPtr::ObjectPtr(const SharedObject& obj) : id(obj.id), ptr(obj.GetRawPtr()) {}

TypeID ObjectPtr::GetType() const {
    return id;
}

void* ObjectPtr::GetPtr() const {
    return ptr;
}

void* ObjectPtr::GetRawPtr() const {
    return ptr;
}

SharedObject ObjectPtr::ToSharedPtr() const {
    return SharedObject{ id, ptr };
}

ObjectPtr ObjectPtr::GetField(std::string_view name) const {
    return ReflMgr::Instance().GetField(*this, name);
}

SharedObject ObjectPtr::Invoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(*this, method, params);
}

SharedObject ObjectPtr::TryInvoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(*this, method, params, false);
}

SharedObject::SharedObject() : id(TypeID::get<void>()), ptr(nullptr), objPtr(nullptr) {};
SharedObject::SharedObject(const SharedObject& other) : id(other.id), ptr(other.ptr), objPtr(other.objPtr) {}
SharedObject::SharedObject(TypeID id, void* objPtr) : id(id), objPtr(objPtr) {}
SharedObject::SharedObject(TypeID id, std::shared_ptr<void> ptr, bool call_ctor) : id(id), ptr(ptr), objPtr(nullptr) {
    if (id == TypeID::get<void>()) {
        return;
    }
    needDestruct = true;
    if (call_ctor) {
        ctor({});
    }
}

TypeID SharedObject::GetType() const {
    return id;
}

std::shared_ptr<void> SharedObject::GetPtr() const {
    return ptr;
}

bool SharedObject::isObjectPtr() const {
    return objPtr != nullptr;
}

void* SharedObject::GetRawPtr() const {
    if (isObjectPtr()) {
        return objPtr;
    }
    return ptr.get();
}

ObjectPtr SharedObject::GetField(std::string_view name) const {
    return ReflMgr::Instance().GetField(*this, name);
}

SharedObject SharedObject::Invoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(*this, method, params);
}

SharedObject SharedObject::TryInvoke(std::string_view method, const std::vector<ObjectPtr>& params) const {
    return ReflMgr::Instance().Invoke(*this, method, params, false);
}

ObjectPtr SharedObject::ToObjectPtr() const {
    if (isObjectPtr()) {
        return ObjectPtr{ id, objPtr };
    }
    return ObjectPtr{ id, ptr.get() };
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
        ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_ctor, args, false);                 \
    }                                                                                                   \
    void T::dtor() const {                                                                              \
        ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_dtor, {}, false);                   \
    }                                                                                                   \
    SharedObject T::copy() const {                                                                      \
        return ReflMgr::Instance().Invoke(*(T*)this, MetaMethods::operator_copy, {});                   \
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
    auto obj = ptr.TryInvoke(MetaMethods::operator_tostring, {});
    if (obj.GetType() != TypeID::get<std::string>()) {
        return out;
    }
    out << obj.Get<std::string>();
    return out;
}

std::ostream& operator << (std::ostream& out, const SharedObject& ptr) {
    auto obj = ptr.TryInvoke(MetaMethods::operator_tostring, {});
    if (obj.GetType() != TypeID::get<std::string>()) {
        return out;
    }
    out << obj.Get<std::string>();
    return out;
}

SharedObject::~SharedObject() {
    if (id == TypeID::get<void>() || !needDestruct) {
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
