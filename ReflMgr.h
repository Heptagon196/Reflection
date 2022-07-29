#pragma once
#include <map>
#include <utility>
#include <queue>
#include "TemplateUtils.h"
#include "Object.h"
#include "TypeID.h"
#include "MetaMethods.h"

using TagList = std::map<std::string, std::vector<std::string>>;

template<typename RegisterFuncType>
struct RegisterTypeInfo {
    std::string name;
    TagList tags;
    std::function<RegisterFuncType> getRegister;
    RegisterTypeInfo() {}
    RegisterTypeInfo(std::string_view name): name(name) {}
    RegisterTypeInfo(std::string_view name, TagList tags): name(name), tags(tags) {}
};

struct FieldInfo : RegisterTypeInfo<ObjectPtr(void*)> {
    using RegisterTypeInfo::RegisterTypeInfo;
    FieldInfo& withRegister(std::function<ObjectPtr(void*)> getRegister) {
        this->getRegister = getRegister;
        return *this;
    }
};

struct ArgsTypeList : std::vector<TypeID> {
    using std::vector<TypeID>::operator=;
    using std::vector<TypeID>::vector;
};

struct MethodInfo : RegisterTypeInfo<SharedObject(void*, const std::vector<void*>&)> {
    TypeID returnType;
    ArgsTypeList argsList;
    using RegisterTypeInfo::RegisterTypeInfo;
    MethodInfo& withRegister(std::function<SharedObject(void*, const std::vector<void*>&)> getRegister) {
        this->getRegister = getRegister;
        return *this;
    }
};

template<typename T>
struct FieldType {
    T type;
    FieldInfo info;
    FieldType(T type, FieldInfo info) : type(type), info(info) {}
    FieldType(T type, std::string_view name) : type(type), info(FieldInfo(name)) {}
};

struct ClassInfo {
    std::vector<TypeID> parents;
    std::vector<std::function<void*(void*)>> cast;
    TagList tags;
    std::function<SharedObject(const std::vector<ObjectPtr>&)> newObject = 0;
};

class ReflMgr {
    private:
        ReflMgr() {}
        std::map<TypeID, std::map<std::string, FieldInfo>> fieldInfo;
        std::map<TypeID, std::map<std::string, std::vector<MethodInfo>>> methodInfo;
        std::map<TypeID, ClassInfo> classInfo;
        template<typename T, typename U>
        auto GetFieldRegisterFunc(T U::* p) {
            return [p](void* instance) {
                return ObjectPtr{ TypeID::get<T>(), (void*)&((U*)(instance)->*p) };
            };
        }
        template<typename T>
        T* SafeGetList(std::map<TypeID, T>& info, TypeID id) {
            if (info.find(id) == info.end()) {
                return nullptr;
            }
            return &info[id];
        }
        template<typename T>
        T* SafeGet(std::map<TypeID, std::map<std::string, T>>& info, TypeID id, std::string_view name) {
            auto* list = SafeGetList(info, id);
            if (list == nullptr) {
                return nullptr;
            }
            if (list->find((std::string)name) == list->end()) {
                return nullptr;
            }
            return &(*list)[(std::string)name];
        }
        bool CheckParams(MethodInfo info, const ArgsTypeList& list) {
            if (info.argsList.size() != list.size()) {
                return false;
            }
            for (int i = 0; i < info.argsList.size(); i++) {
                if (!list[i].canBeAppliedTo(info.argsList[i])) {
                    return false;
                }
            }
            return true;
        }
        const MethodInfo* SafeGet(TypeID id, std::string_view name, const ArgsTypeList& args) {
            auto* list = SafeGet(methodInfo, id, name);
            if (list == nullptr) {
                return nullptr;
            }
            for (const auto& info : *list) {
                if (CheckParams(info, args)) {
                    return &info;
                }
            }
            return nullptr;
        }
        template<typename Ret> Ret* WalkThroughInherits(void** instance, TypeID id, std::function<Ret*(TypeID)> func) {
            std::queue<std::pair<TypeID, void*>> q;
            q.push({id, instance == nullptr ? nullptr : *instance});
            while (!q.empty()) {
                auto [type, ptr] = q.front();
                q.pop();
                auto* ret = func(type);
                if (ret != nullptr) {
                    if (instance != nullptr) {
                        *instance = ptr;
                    }
                    return ret;
                }
                if (HasClassInfo(type)) {
                    ClassInfo& info = classInfo[type];
                    for (int i = 0; i < info.parents.size(); i++) {
                        q.push({info.parents[i], instance == nullptr ? nullptr : info.cast[i](ptr)});
                    }
                }
            }
            return nullptr;
        }
        const FieldInfo* SafeGetFieldWithInherit(void** instance, TypeID id, std::string_view name, bool showError = true) {
            auto ret = WalkThroughInherits(instance, id, std::function([&](TypeID type) { return SafeGet(fieldInfo, type, name); }));
            if (ret == nullptr && showError) {
                std::cerr << "Error: no matching field found: " << id.getName() << "::" << name << std::endl;
            }
            return ret;
        }
        const MethodInfo* SafeGetMethodWithInherit(void** instance, TypeID id, std::string_view name, const ArgsTypeList& args, bool showError = true) {
            auto ret = WalkThroughInherits(instance, id, std::function([&](TypeID type) { return SafeGet(type, name, args); }));
            if (ret == nullptr && showError) {
                std::cerr << "Error: no matching method found: " << id.getName() << "::" << name << "(";
                if (args.size() > 0) {
                    std::cerr << args[0].getName();
                    for (int i = 1; i < args.size(); i++) {
                        std::cerr << ", " << args[i].getName();
                    }
                }
                std::cerr << ")" << std::endl;
            }
            return ret;
        }
        bool HasClassInfo(TypeID type) {
            return classInfo.find(type) != classInfo.end();
        }
    public:
        ReflMgr(const ReflMgr&) = delete;
        ReflMgr(ReflMgr&&) = delete;
        ReflMgr(ReflMgr&) = delete;
        static ReflMgr& Instance() {
            static ReflMgr instance;
            return instance;
        }
        SharedObject New(TypeID type, const std::vector<ObjectPtr>& args) {
            auto& func = classInfo[type].newObject;
            if (func == 0) {
                std::cerr << "Error: unable to init an unregistered class: " << type.getName() << std::endl;
                return SharedObject::Null;
            }
            return func(args);
        }
        template<typename T>
        SharedObject New(const std::vector<ObjectPtr>& args) {
            return New(TypeID::get<T>(), args);
        }
        SharedObject New(TypeID type) {
            return New(type, {});
        }
        template<typename T>
        SharedObject New() {
            return New(TypeID::get<T>());
        }
        template<typename T, typename U>
        void AddField(U T::* type, FieldInfo info) {
            fieldInfo[TypeID::get<T>()][info.name] = info.withRegister(GetFieldRegisterFunc(type));
        }
        template<typename T, typename U>
        void AddField(U T::* type, std::string_view name) {
            FieldInfo info{ name };
            AddField(type, info);
        }
        template<typename T, typename U>
        void AddField(FieldType<U T::*> field) {
            AddField(field.type, field.info);
        }
        template<typename T0, typename U, typename... T>
        void AddField(FieldType<U T0::*> member, T... args) {
            AddField<T0, U>(member);
            AddField<T0>(args...);
        }
        template<typename T>
        void AddStaticField(TypeID type, T* ptr, FieldInfo info) {
            fieldInfo[type][(std::string)info.name] = info.withRegister([ptr](void*) -> SharedObject {
                return SharedObject{ TypeID::get<T>(), (void*)ptr };
            });
        }
        template<typename T>
        void AddStaticField(TypeID type, T* ptr, std::string_view name) {
            FieldInfo info{ name };
            AddStaticField(type, ptr, info);
        }
        template<typename T>
        void AddStaticField(TypeID type, FieldType<T> info) {
            AddStaticField(type, info.type, info.info);
        }
        template<typename T>
        void AddStaticField(TypeID type) {}
        template<typename T0, typename... T>
        void AddStaticField(TypeID type, FieldType<T0*> info, T... args) {
            AddStaticField<T0>(type, info.type, info.info);
            AddStaticField<T0>(type, args...);
        }
        ObjectPtr RawGetField(TypeID type, void* instance, std::string_view member) {
            auto* p = SafeGetFieldWithInherit(&instance, type, member);
            if (p == nullptr) {
                return ObjectPtr::Null;
            }
            return p->getRegister(instance);
        }
        template<typename T>
        ObjectPtr GetField(T instance, std::string_view member) {
            return RawGetField(instance.GetType(), instance.GetRawPtr(), member);
        }
#define DEF_METHOD_REG(end)                                                                                                     \
    private:                                                                                                                    \
        template<typename Ret, typename Type, typename... Args, size_t... N>                                                    \
        auto GetMethodRegisterFunc(Ret (Type::* p)(Args...) end, std::index_sequence<N...> is) {                                \
            return [p](void* instance, const std::vector<void*>& params) -> SharedObject {                                      \
                return SharedObject{ TypeID::get<Ret>(),                                                                        \
                    std::shared_ptr<void>(                                                                                      \
                        new Ret(((Type*)instance->*p)(                                                                          \
                            (                                                                                                   \
                                *(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))                     \
                            )...                                                                                                \
                        ))                                                                                                      \
                    )                                                                                                           \
                };                                                                                                              \
            };                                                                                                                  \
        }                                                                                                                       \
        template<typename Type, typename... Args, size_t... N>                                                                  \
        auto GetMethodRegisterFunc(void (Type::* p)(Args...) end, std::index_sequence<N...> is) {                               \
            return [p](void* instance, const std::vector<void*>& params) -> SharedObject {                                      \
                ((Type*)instance->*p)(                                                                                          \
                    (*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N])))...                            \
                );                                                                                                              \
                return SharedObject{TypeID::get<void>(), nullptr};                                                              \
            };                                                                                                                  \
        }                                                                                                                       \
        template<typename Ret, typename Type, typename... Args, size_t... N>                                                    \
        auto GetMethodRegisterFunc(Ret& (Type::* p)(Args...) end, std::index_sequence<N...> is) {                               \
            return [p](void* instance, const std::vector<void*>& params) -> SharedObject {                                      \
                return SharedObject{ TypeID::get<Ret&>(),                                                                       \
                    (void*)(                                                                                                    \
                        &(((Type*)instance->*p)(                                                                                \
                            (                                                                                                   \
                                *(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))                     \
                            )...                                                                                                \
                        ))                                                                                                      \
                    )                                                                                                           \
                };                                                                                                              \
            };                                                                                                                  \
        }                                                                                                                       \
        template<typename Ret, typename Type, typename... Args>                                                                 \
        auto GetMethodRegisterFunc(Ret (Type::* p)(Args...) end) {                                                              \
            return GetMethodRegisterFunc(std::forward<decltype(p)>(p),                                                          \
                std::make_index_sequence<Count<TypeList<Args...>>::count>()                                                     \
            );                                                                                                                  \
        }                                                                                                                       \
    public:                                                                                                                     \
        template<typename Ret, typename Type, typename... Args>                                                                 \
        void AddMethod(Ret (Type::* func)(Args...) end, MethodInfo info) {                                                      \
            info.returnType = TypeID::get<Ret>();                                                                               \
            info.argsList = ArgsTypeList{TypeID::get<Args>()...};                                                               \
            methodInfo[TypeID::get<Type>()][(std::string)info.name].push_back(                                                  \
                info.withRegister(                                                                                              \
                    GetMethodRegisterFunc(func)                                                                                 \
                )                                                                                                               \
            );                                                                                                                  \
        }                                                                                                                       \
        template<typename Ret, typename Type, typename... Args>                                                                 \
        void AddMethod(Ret (Type::* func)(Args...) end, std::string_view name) {                                                \
            MethodInfo info{ name };                                                                                            \
            AddMethod(func, info);                                                                                              \
        }
        template<typename Type, typename Ret, typename... Args>
        void AddMethod(std::function<Ret(Type*, Args...)> func, MethodInfo info) {
            info.returnType = TypeID::get<Ret>();
            info.argsList = ArgsTypeList{TypeID::get<Args>()...};
            methodInfo[TypeID::get<Type>()][(std::string)info.name].push_back(
                info.withRegister(GetLambdaRegisterFunc(func))
            );
        }
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, std::function<Ret(Args...)> func, MethodInfo info) {
            info.returnType = TypeID::get<Ret>();
            info.argsList = ArgsTypeList{TypeID::get<Args>()...};
            methodInfo[type][(std::string)info.name].push_back(
                info.withRegister(GetLambdaRegisterStaticFunc(func))
            );
        }
        template<typename Type, typename Ret, typename... Args>
        void AddMethod(std::function<Ret(Type*, Args...)> func, std::string_view name) {
            MethodInfo info{ name };
            AddMethod(func, info);
        }
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, std::function<Ret(Args...)> func, std::string_view name) {
            MethodInfo info{ name };
            AddStaticMethod(type, func, info);
        }
        DEF_METHOD_REG()
        DEF_METHOD_REG(const)
#undef DEF_METHOD_REG
    private:
        template<typename Type, typename Ret, typename... Args, size_t... N>
        auto GetLambdaRegisterFunc(std::function<Ret(Type*, Args...)> func, std::index_sequence<N...> is) {
            return [func](void* instance, const std::vector<void*>& params) -> SharedObject {
                return SharedObject{ TypeID::get<Ret>(),
                    std::shared_ptr<void>(
                        new Ret(func((Type*)instance, *(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...))
                    )
                };
            };
        }
        template<typename Type, typename... Args, size_t... N>
        auto GetLambdaRegisterFunc(std::function<void(Type*, Args...)> func, std::index_sequence<N...> is) {
            return [func](void* instance, const std::vector<void*>& params) -> SharedObject {
                func((Type*)instance, *(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
                return SharedObject{ TypeID::get<void>(), nullptr };
            };
        }
        template<typename Type, typename Ret, typename... Args, size_t... N>
        auto GetLambdaRegisterFunc(std::function<Ret&(Type*, Args...)> func, std::index_sequence<N...> is) {
            return [func](void* instance, const std::vector<void*>& params) -> SharedObject {
                return SharedObject{ TypeID::get<Ret&>(),
                    (void*)(
                        &(func((Type*)instance, *(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...))
                    )
                };
            };
        }
        template<typename Type, typename Ret, typename... Args>
        auto GetLambdaRegisterFunc(std::function<Ret(Type*, Args...)> func) {
            return GetLambdaRegisterFunc(std::forward<decltype(func)>(func),
                std::make_index_sequence<Count<TypeList<Args...>>::count>()
            );
        }
        template<typename Ret, typename... Args, size_t... N>
        auto GetLambdaRegisterStaticFunc(std::function<Ret(Args...)> func, std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params) -> SharedObject {
                return SharedObject{ TypeID::get<Ret>(),
                    std::shared_ptr<void>(
                        new Ret(func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...))
                    )
                };
            };
        }
        template<typename... Args, size_t... N>
        auto GetLambdaRegisterStaticFunc(std::function<void(Args...)> func, std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params) -> SharedObject {
                func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
                return SharedObject{ TypeID::get<void>(), nullptr };
            };
        }
        template<typename Ret, typename... Args, size_t... N>
        auto GetLambdaRegisterStaticFunc(std::function<Ret&(Args...)> func, std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params) -> SharedObject {
                return SharedObject{ TypeID::get<Ret&>(),
                    (void*)(
                        &(func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...))
                    )
                };
            };
        }
        template<typename Ret, typename... Args>
        auto GetLambdaRegisterStaticFunc(std::function<Ret(Args...)> func) {
            return GetLambdaRegisterStaticFunc(std::forward<decltype(func)>(func),
                std::make_index_sequence<Count<TypeList<Args...>>::count>()
            );
        }
        template<typename Ret, typename... Args, size_t... N>
        auto GetStaticMethodRegisterFunc(Ret (*func)(Args...), std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params) -> SharedObject {
                return SharedObject{ TypeID::get<Ret>(),
                    std::shared_ptr<void>(
                        new Ret(func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...))
                    )
                };
            };
        }
        template<typename... Args, size_t... N>
        auto GetStaticMethodRegisterFunc(void (*func)(Args...), std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params) -> SharedObject {
                func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
                return SharedObject{ TypeID::get<void>(), nullptr };
            };
        }
        template<typename Ret, typename... Args, size_t... N>
        auto GetStaticMethodRegisterFunc(Ret& (*func)(Args...), std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params) -> SharedObject {
                return SharedObject{ TypeID::get<Ret&>(),
                    (void*)(
                        &(func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...))
                    )
                };
            };
        }
        template<typename Ret, typename... Args>
        auto GetStaticMethodRegisterFunc(Ret (* func)(Args...)) {
            return GetStaticMethodRegisterFunc(std::forward<decltype(func)>(func),
                std::make_index_sequence<Count<TypeList<Args...>>::count>()
            );
        }
        template<typename T>
        std::vector<void*> ConvertParams(const std::vector<T>& params, const MethodInfo& info, std::vector<std::shared_ptr<void>>& temp) {
            std::vector<void*> ret;
            for (int i = 0; i < info.argsList.size(); i++) {
                if (info.argsList[i].getHash() != params[i].GetType().getHash()) {
                    temp.push_back(params[i].GetType().implicitConvertInstance(params[i].GetRawPtr(), info.argsList[i]));
                    ret.push_back(temp[temp.size() - 1].get());
                    continue;
                }
                ret.push_back(params[i].GetRawPtr());
            }
            return ret;
        }
    public:
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, Ret (*func)(Args...), MethodInfo info) {
            info.returnType = TypeID::get<Ret>();
            info.argsList = ArgsTypeList{TypeID::get<Args>()...};
            methodInfo[type][(std::string)info.name].push_back(
                info.withRegister(
                    GetStaticMethodRegisterFunc(std::forward<decltype(func)>(func))
                )
            );
        }
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, Ret (*func)(Args...), std::string_view name) {
            MethodInfo info{ name };
            AddStaticMethod(type, func, info);
        }
        SharedObject RawInvoke(TypeID type, void* instance, std::string_view member, ArgsTypeList list, std::vector<void*> params) {
            auto* info = SafeGetMethodWithInherit(&instance, type, member, list);
            if (info == nullptr || info->name == "") {
                return SharedObject::Null;
            }
            return info->getRegister(instance, params);
        }
        template<typename T = ObjectPtr, typename U>
        SharedObject Invoke(U instance, std::string_view method, const std::vector<T>& params, bool showError = true) {
            ArgsTypeList list;
            for (auto param : params) {
                list.push_back(param.GetType());
            }
            void* ptr = instance.GetRawPtr();
            auto* info = SafeGetMethodWithInherit(&ptr, instance.GetType(), method, list, showError);
            if (info == nullptr || info->name == "") {
                return SharedObject::Null;
            }
            std::vector<std::shared_ptr<void>> temp;
            return info->getRegister(ptr, ConvertParams(params, *info, temp));
        }
        template<typename T = ObjectPtr>
        SharedObject InvokeStatic(TypeID type, std::string_view method, const std::vector<T>& params, bool showError = true) {
            ArgsTypeList list;
            for (auto param : params) {
                list.push_back(param.GetType());
            }
            auto* info = SafeGetMethodWithInherit(nullptr, type, method, list, showError);
            if (info == nullptr) {
                return SharedObject::Null;
            }
            std::vector<std::shared_ptr<void>> temp;
            return info->getRegister(nullptr, ConvertParams(params, *info, temp));
        }
        template<typename D, typename B>
        void SetInheritance() {
            ClassInfo& info = classInfo[TypeID::get<D>()];
            info.parents.push_back(TypeID::get<B>());
            info.cast.push_back([](void* derived) -> void* {
                return (B*)((D*)derived);
            });
        }
        template<typename D, typename B, typename R, typename... Args>
        void SetInheritance() {
            SetInheritance<D, B>();
            SetInheritance<D, R, Args...>();
        }
        template<typename T>
        void AddClass(TagList tagList) {
            auto type = TypeID::get<T>();
            classInfo[type].newObject = [](const std::vector<ObjectPtr>& args) -> SharedObject {
                auto obj = SharedObject{TypeID::get<T>(), std::make_shared<T>(), false};
                obj.ctor(args);
                return obj;
            };
            classInfo[type].tags = tagList;
        }
        template<typename T>
        void AddClass() {
            AddClass<T>({});
        }

    private:
        // functions to be exported
        void ExportAddField(TypeID cls, std::string_view name, std::function<ObjectPtr(ObjectPtr)> func) {
            FieldInfo info{ name };
            fieldInfo[cls][(std::string)name] = info.withRegister([func, cls](void* ptr) { return func(ObjectPtr{cls, ptr}); });
        }
        void ExportAddStaticField(TypeID cls, std::string_view name, std::function<ObjectPtr()> func) {
            FieldInfo info{ name };
            fieldInfo[cls][(std::string)name] = info.withRegister([func, cls](void* ptr) { return func(); });
        }
        ObjectPtr ExportGetField(ObjectPtr instance, std::string_view name) {
            return RawGetField(instance.GetType(), instance.GetRawPtr(), name);
        }
        void RawAddMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(ObjectPtr, const std::vector<ObjectPtr>&)> func) {
            MethodInfo info{ name };
            info.returnType = returnType;
            info.argsList = argsList;
            methodInfo[cls][(std::string)name].push_back(info.withRegister([func, returnType, argsList, cls](void* instance, const std::vector<void*>& params) -> SharedObject {
                std::vector<ObjectPtr> args;
                for (int i = 0; i < params.size(); i++) {
                    args.push_back(ObjectPtr{argsList[i], params[i]});
                }
                return func(ObjectPtr{returnType, instance}, args);
            }));
        }
        SharedObject ExportInvoke(ObjectPtr instance, std::string_view name, const std::vector<ObjectPtr>& params) {
            ArgsTypeList list;
            std::vector<void*> args;
            for (auto param : params) {
                args.push_back(param.GetRawPtr());
                list.push_back(param.GetType());
            }
            return RawInvoke(instance.GetType(), instance.GetRawPtr(), name, list, args);
        }
        void ExportAddStaticMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(const std::vector<ObjectPtr>&)> func) {
            MethodInfo info{ name };
            info.returnType = returnType;
            info.argsList = argsList;
            methodInfo[cls][(std::string)name].push_back(info.withRegister([func, returnType, argsList, cls](void* instance, const std::vector<void*>& params) -> SharedObject {
                std::vector<ObjectPtr> args;
                for (int i = 0; i < params.size(); i++) {
                    args.push_back(ObjectPtr{argsList[i], params[i]});
                }
                return func(args);
            }));
        }
        SharedObject ExportInvokeStatic(TypeID type, std::string_view name, const std::vector<ObjectPtr>& params) {
            return InvokeStatic(type, name, params);
        }
    public:
        void SelfExport() {
            auto& mgr = Instance();
            mgr.AddStaticMethod(TypeID::get<ReflMgr>(), &ReflMgr::Instance, "Instance");
            mgr.AddMethod(&ReflMgr::ExportAddField, "AddField");
            mgr.AddMethod(&ReflMgr::ExportAddStaticField, "AddStaticField");
            mgr.AddMethod(&ReflMgr::ExportGetField, "GetField");
            mgr.AddMethod(&ReflMgr::RawAddMethod, "AddMethod");
            mgr.AddMethod(&ReflMgr::ExportInvoke, "Invoke");
            mgr.AddMethod(&ReflMgr::ExportAddStaticMethod, "AddStaticMethod");
            mgr.AddMethod(&ReflMgr::ExportInvokeStatic, "InvokeStatic");
        }
};
