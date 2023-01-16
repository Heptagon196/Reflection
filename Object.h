#pragma once
#include <memory>
#include <variant>
#include "TypeID.h"
#include "MetaMethods.h"

class ObjectPtr;
class SharedObject;

#define DEFOPS(T)       \
    BIDEF(T, +)         \
    BIDEF(T, -)         \
    BIDEF(T, *)         \
    BIDEF(T, /)         \
    BIDEF(T, %)         \
    BIDEF(T, ^)         \
    BIDEF(T, ==)        \
    BIDEF(T, !=)        \
    BIDEF(T, <)         \
    BIDEF(T, <=)        \
    BIDEF(T, >)         \
    BIDEF(T, >=)        \
    SharedObject operator - () const;                                           \
    SharedObject operator [] (ObjectPtr idx) const;                             \
    SharedObject operator () (const std::vector<ObjectPtr>& args) const;        \
    SharedObject tostring() const;                                              \
    void ctor(const std::vector<ObjectPtr>& args) const;                        \
    void dtor() const;                                                          \
    SharedObject begin();                                                       \
    SharedObject end();                                                         \
    SharedObject operator ++ (int);                                             \
    SharedObject operator -- (int);                                             \
    SharedObject operator ++ ();                                                \
    SharedObject operator -- ();                                                \
    SharedObject operator * ();                                                 \

#define BIDEF(T, op)                                    \
    SharedObject operator op (const T& other) const;

template<typename T> struct ToObject;
template<typename T> struct ToShared;

struct TemplatedTypeID;

struct ArgsTypeList : std::vector<TemplatedTypeID> {
    using std::vector<TemplatedTypeID>::operator=;
    using std::vector<TemplatedTypeID>::vector;
};

struct TemplatedTypeID {
    TypeID base;
    ArgsTypeList args;
    TemplatedTypeID();
    TemplatedTypeID(TypeID type);
    std::string getName() const;
};

struct ObjectInfo {
    ObjectInfo(TypeID type);
    ObjectInfo(TemplatedTypeID type);
    TemplatedTypeID id;
};

class ObjectPtr {
    private:
        std::shared_ptr<ObjectInfo> info;
        void* content;
    public:
        DEFOPS(ObjectPtr)
        static const ObjectPtr Null;
        ObjectPtr();
        ObjectPtr(const ObjectPtr& other);
        ObjectPtr(std::shared_ptr<ObjectInfo> info, void* ptr);
        ObjectPtr(TemplatedTypeID id, void* ptr);
        ObjectPtr(const SharedObject& obj);
        TemplatedTypeID& GetType() const;
        void* GetPtr() const;
        void* GetRawPtr() const;
        ObjectPtr GetField(std::string_view member) const;
        SharedObject Invoke(std::string_view method, const std::vector<ObjectPtr>& params = {}) const;
        SharedObject TryInvoke(std::string_view method, const std::vector<ObjectPtr>& params = {}) const;
        template<typename T> T& As() { return *(T*)content; }
        template<typename T> const T& Get() const { return *(T*)content; }
        friend class SharedObject;
        friend struct ToShared<ObjectPtr>;
        friend std::ostream& operator << (std::ostream& out, const ObjectPtr& ptr);
        template<typename T> ObjectPtr& operator = (const T& other) {
            *this = ToObject<T>()(other);
            return *this;
        }
        operator bool() const;
};
 
class SharedObject {
    private:
        std::shared_ptr<ObjectInfo> info;
        std::shared_ptr<void> content;
        void* fallbackPtr;
    public:
        template<typename T, typename... Args>
        static SharedObject New(Args&&... args) {
            return SharedObject{TypeID::get<T>(), std::make_shared<T>(std::forward<Args&&>(args)...)};
        }
        template<typename T>
        static SharedObject New(const T& val) {
            return SharedObject{TypeID::get<T>(), std::make_shared<T>(val)};
        }
        DEFOPS(SharedObject)
        static const SharedObject Null;
        SharedObject();
        SharedObject(const SharedObject& other);
        SharedObject(TypeID type, void* objPtr);
        SharedObject(std::shared_ptr<ObjectInfo> info, void* objPtr);
        SharedObject(const TemplatedTypeID& id, std::shared_ptr<void> ptr, bool call_ctor = true);
        SharedObject& AddTemplateArgs(const ArgsTypeList& templateArgs);
        bool isObjectPtr() const;
        TemplatedTypeID& GetType() const;
        std::shared_ptr<void> GetPtr() const;
        void* GetRawPtr() const;
        ObjectPtr GetField(std::string_view member) const;
        SharedObject Invoke(std::string_view method, const std::vector<ObjectPtr>& params = {}) const;
        SharedObject TryInvoke(std::string_view method, const std::vector<ObjectPtr>& params = {}) const;
        template<typename T> T& As() { return *(T*)GetRawPtr(); }
        template<typename T> const T& Get() const { return *(T*)GetRawPtr(); }
        ObjectPtr ToObjectPtr() const;
        ~SharedObject();
        friend class ObjectPtr;
        friend std::ostream& operator << (std::ostream& out, const SharedObject& ptr);
        template<typename T> SharedObject& operator = (const T& other) {
            *this = ToShared<T>()(other);
            return *this;
        }
        operator bool() const;
};

template<typename T>
struct ToObject { ObjectPtr operator()(const T& other) { return SharedObject::New<T>(other).ToObjectPtr(); } };
template<>
struct ToObject<SharedObject> { ObjectPtr operator()(const SharedObject& other) { return other.ToObjectPtr(); }};
template<>
struct ToObject<ObjectPtr> { ObjectPtr operator()(const ObjectPtr& other) { return other; }};
template<typename T>
struct ToShared { SharedObject operator()(const T& other) { return SharedObject::New<T>(other); }};
template<>
struct ToShared<SharedObject> { SharedObject operator()(const SharedObject& other) { return other; }};
template<>
struct ToShared<ObjectPtr> { SharedObject operator()(const ObjectPtr& other) { return SharedObject{ other.info, other.content }; }};

class Namespace {
    private:
        TypeID space;
        SharedObject vObj;
    public:
        TypeID Type() const;
        static const Namespace Global;
        Namespace(std::string_view space);
        Namespace getNamespace(std::string_view subSpace) const;
        TypeID getClass(std::string_view clsName) const;
        SharedObject Invoke(std::string_view method, const std::vector<ObjectPtr>& params) const;
};

#undef DEFOPS
#undef BIDEF
