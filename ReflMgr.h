#pragma once
#include <utility>
#include <queue>
#include <functional>
#include "TemplateUtils.h"
#include "Object.h"
#include "TypeID.h"
#include "MetaMethods.h"

using TagList = std::unordered_map<std::string, std::vector<std::string>>;

struct FieldInfo {
    std::string name;
    TagList tags;
    std::function<ObjectPtr(void*)> getRegister;
    FieldInfo& withRegister(std::function<ObjectPtr(void*)> getRegister);
};

struct ArgsTypeList : std::vector<TypeID> {
    using std::vector<TypeID>::operator=;
    using std::vector<TypeID>::vector;
};

struct MethodInfo {
    using FuncType = void(void*, const std::vector<void*>&, SharedObject&);
    std::string name;
    TagList tags;
    std::function<FuncType> getRegister;
    std::function<SharedObject()> newRet;
    TypeID returnType;
    ArgsTypeList argsList;
    MethodInfo& withRegister(std::function<FuncType> getRegister);
    bool sameDeclareTo(const MethodInfo& other) const;
};

template<typename T>
struct FieldType {
    T type;
    FieldInfo info;
    FieldType(T type, FieldInfo info) : type(type), info(info) {}
    FieldType(T type, std::string_view name) : type(type), info({std::string{name}}) {}
};

struct ClassInfo {
    TypeID aliasTo;
    std::vector<TypeID> parents;
    std::vector<std::function<void*(void*)>> cast;
    TagList tags;
    std::function<SharedObject(const std::vector<ObjectPtr>&)> newObject = 0;
};

class ReflMgr {
    private:
        ReflMgr() {}
        std::string errorMsgPrefix;
        TypeIDMap<std::unordered_map<std::string, FieldInfo>> fieldInfo;
        TypeIDMap<std::unordered_map<std::string, std::vector<MethodInfo>>> methodInfo;
        TypeIDMap<ClassInfo> classInfo;
        template<typename T, typename U>
        auto GetFieldRegisterFunc(T U::* p) {
            return [p](void* instance) {
                return ObjectPtr{ TypeID::get<T>(), (void*)&((U*)(instance)->*p) };
            };
        }
        template<typename T> T* SafeGetList(TypeIDMap<T>& info, TypeID id);
        template<typename T> T* SafeGet(TypeIDMap<std::unordered_map<std::string, T>>& info, TypeID id, std::string_view name);
        bool CheckParams(MethodInfo info, const ArgsTypeList& list);
        const MethodInfo* SafeGet(TypeID id, std::string_view name, const ArgsTypeList& args);
        template<typename Ret> Ret* WalkThroughInherits(std::function<void*(void*)>* instanceConv, TypeID id, std::function<Ret*(TypeID)> func);
        const FieldInfo* SafeGetFieldWithInherit(std::function<void*(void*)>* instanceConv, TypeID id, std::string_view name, bool showError = true);
        const MethodInfo* SafeGetMethodWithInherit(std::function<void*(void*)>* instanceConv, TypeID id, std::string_view name, const ArgsTypeList& args, bool showError = true);
        void AddMethodInfo(TypeID type, const std::string& name, MethodInfo info, bool overridePrevious = true);
    public:
        ReflMgr(const ReflMgr&) = delete;
        ReflMgr(ReflMgr&&) = delete;
        ReflMgr(ReflMgr&) = delete;
        void SetErrorMsgPrefix(const std::string& msg);
        static ReflMgr& Instance();
        static TypeID GetType(std::string_view clsName);
        bool HasClassInfo(TypeID type);
        SharedObject New(TypeID type, const std::vector<ObjectPtr>& args = {});
        SharedObject New(std::string_view typeName, const std::vector<ObjectPtr>& args = {});
        template<typename T>
        SharedObject New(const std::vector<ObjectPtr>& args = {}) {
            return New(TypeID::get<T>(), args);
        }
        template<typename T, typename U>
        void AddField(U T::* type, FieldInfo info) {
            fieldInfo[TypeID::get<T>()][info.name] = info.withRegister(GetFieldRegisterFunc(type));
        }
        template<typename T, typename U>
        void AddField(U T::* type, std::string_view name) {
            FieldInfo info{ std::string{name} };
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
            fieldInfo[type][std::string{info.name}] = info.withRegister([ptr](void*) -> SharedObject {
                return SharedObject{ TypeID::get<T>(), (void*)ptr };
            });
        }
        template<typename T>
        void AddStaticField(TypeID type, T* ptr, std::string_view name) {
            FieldInfo info{ std::string{name} };
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
        ObjectPtr RawGetField(TypeID type, void* instance, std::string_view member);
        template<typename T>
        ObjectPtr GetField(T instance, std::string_view member) {
            return RawGetField(instance.GetType(), instance.GetRawPtr(), member);
        }
    private:
        template<typename Func>
        auto GetNewRet(Func func) {
            using Ret = typename MethodTraits<Func>::ReturnType;
            if constexpr (std::is_same<void, Ret>() || std::is_reference<Ret>()) {
                return []() { return SharedObject(); };
            } else {
                return []() { return SharedObject::New<Ret>(); };
            }
        }
        template<typename Ret, typename... Args>
        auto GetNewRet(std::function<Ret(Args...)> func) {
            if constexpr (std::is_same<void, Ret>() || std::is_reference<Ret>()) {
                return []() { return SharedObject(); };
            } else {
                return []() { return SharedObject::New<Ret>(); };
            }
        }
    public:
#define DEF_METHOD_REG(end)                                                                                                     \
    private:                                                                                                                    \
        template<typename Ret, typename Type, typename... Args, size_t... N>                                                    \
        auto GetMethodRegisterFunc(Ret (Type::* p)(Args...) end, std::index_sequence<N...> is) {                                \
            return [p](void* instance, const std::vector<void*>& params, SharedObject& ret) {                                   \
                ret.As<Ret>() =                                                                                                 \
                    ((Type*)instance->*p)(                                                                                      \
                        (                                                                                                       \
                            *(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))                         \
                        )...                                                                                                    \
                    );                                                                                                          \
            };                                                                                                                  \
        }                                                                                                                       \
        template<typename Type, typename... Args, size_t... N>                                                                  \
        auto GetMethodRegisterFunc(void (Type::* p)(Args...) end, std::index_sequence<N...> is) {                               \
            return [p](void* instance, const std::vector<void*>& params, SharedObject& ret) {                                   \
                ((Type*)instance->*p)(                                                                                          \
                    (*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N])))...                            \
                );                                                                                                              \
            };                                                                                                                  \
        }                                                                                                                       \
        template<typename Ret, typename Type, typename... Args, size_t... N>                                                    \
        auto GetMethodRegisterFunc(Ret& (Type::* p)(Args...) end, std::index_sequence<N...> is) {                               \
            return [p](void* instance, const std::vector<void*>& params, SharedObject& ret) {                                   \
                ret = SharedObject{ TypeID::get<Ret&>(),                                                                        \
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
            info.newRet = GetNewRet(func);                                                                                      \
            AddMethodInfo(TypeID::get<Type>(), info.name, info.withRegister(GetMethodRegisterFunc(func)));                      \
        }                                                                                                                       \
        template<typename Ret, typename Type, typename... Args>                                                                 \
        void AddMethod(Ret (Type::* func)(Args...) end, std::string_view name) {                                                \
            MethodInfo info{ std::string{name} };                                                                               \
            AddMethod(func, info);                                                                                              \
        }
        template<typename Type, typename Ret, typename... Args>
        void AddMethod(std::function<Ret(Type*, Args...)> func, MethodInfo info) {
            info.returnType = TypeID::get<Ret>();
            info.argsList = ArgsTypeList{TypeID::get<Args>()...};
            info.newRet = GetNewRet(func);
            AddMethodInfo(TypeID::get<Type>(), info.name, info.withRegister(GetLambdaRegisterFunc(func)));
        }
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, std::function<Ret(Args...)> func, MethodInfo info) {
            info.returnType = TypeID::get<Ret>();
            info.argsList = ArgsTypeList{TypeID::get<Args>()...};
            info.newRet = GetNewRet(func);
            AddMethodInfo(type, info.name, info.withRegister(GetLambdaRegisterStaticFunc(func)));
        }
        template<typename Type, typename Ret, typename... Args>
        void AddMethod(std::function<Ret(Type*, Args...)> func, std::string_view name) {
            MethodInfo info{ std::string{name} };
            AddMethod(func, info);
        }
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, std::function<Ret(Args...)> func, std::string_view name) {
            MethodInfo info{ std::string{name} };
            AddStaticMethod(type, func, info);
        }
        DEF_METHOD_REG()
        DEF_METHOD_REG(const)
#undef DEF_METHOD_REG
    private:
        template<typename Type, typename Ret, typename... Args, size_t... N>
        auto GetLambdaRegisterFunc(std::function<Ret(Type*, Args...)> func, std::index_sequence<N...> is) {
            return [func](void* instance, const std::vector<void*>& params, SharedObject& ret) {
                ret.As<Ret>() = func((Type*)instance, *(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
            };
        }
        template<typename Type, typename... Args, size_t... N>
        auto GetLambdaRegisterFunc(std::function<void(Type*, Args...)> func, std::index_sequence<N...> is) {
            return [func](void* instance, const std::vector<void*>& params, SharedObject& ret) {
                func((Type*)instance, *(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
                ret = SharedObject();
            };
        }
        template<typename Type, typename Ret, typename... Args, size_t... N>
        auto GetLambdaRegisterFunc(std::function<Ret&(Type*, Args...)> func, std::index_sequence<N...> is) {
            return [func](void* instance, const std::vector<void*>& params, SharedObject& ret) {
                ret = SharedObject{ TypeID::get<Ret&>(),
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
            return [func](void*, const std::vector<void*>& params, SharedObject& ret) {
                ret.As<Ret>() = func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
            };
        }
        template<typename... Args, size_t... N>
        auto GetLambdaRegisterStaticFunc(std::function<void(Args...)> func, std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params, SharedObject& ret) {
                func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
            };
        }
        template<typename Ret, typename... Args, size_t... N>
        auto GetLambdaRegisterStaticFunc(std::function<Ret&(Args...)> func, std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params, SharedObject& ret) {
                ret = SharedObject{ TypeID::get<Ret&>(),
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
            return [func](void*, const std::vector<void*>& params, SharedObject& ret) {
                ret.As<Ret>() = func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
            };
        }
        template<typename... Args, size_t... N>
        auto GetStaticMethodRegisterFunc(void (*func)(Args...), std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params, SharedObject& ret) {
                func(*(reinterpret_cast<typename std::remove_reference<Args>::type*>(params[N]))...);
            };
        }
        template<typename Ret, typename... Args, size_t... N>
        auto GetStaticMethodRegisterFunc(Ret& (*func)(Args...), std::index_sequence<N...> is) {
            return [func](void*, const std::vector<void*>& params, SharedObject& ret) {
                ret = SharedObject{ TypeID::get<Ret&>(),
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
            info.newRet = GetNewRet(func);
            AddMethodInfo(type, info.name, info.withRegister(GetStaticMethodRegisterFunc(std::forward<decltype(func)>(func))));
        }
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, Ret (*func)(Args...), std::string_view name) {
            MethodInfo info{ std::string{name} };
            AddStaticMethod(type, func, info);
        }
        std::function<void(void*, std::vector<void*>, SharedObject&)> GetInvokeFunc(TypeID type, std::string_view member, ArgsTypeList list);
        SharedObject RawInvoke(TypeID type, void* instance, std::string_view member, ArgsTypeList list, std::vector<void*> params);
        template<typename T = ObjectPtr, typename U>
        SharedObject Invoke(U instance, std::string_view method, const std::vector<T>& params, bool showError = true) {
            ArgsTypeList list;
            for (auto param : params) {
                list.push_back(param.GetType());
            }
            void* ptr = instance.GetRawPtr();
            std::function<void*(void*)> conv = [](void* orig) { return orig; };
            auto* info = SafeGetMethodWithInherit(&conv, instance.GetType(), method, list, showError);
            ptr = conv(ptr);
            if (info == nullptr || info->name == "") {
                return SharedObject();
            }
            std::vector<std::shared_ptr<void>> temp;
            auto ret = info->newRet();
            info->getRegister(ptr, ConvertParams(params, *info, temp), ret);
            return ret;
        }
        template<typename T = ObjectPtr>
        SharedObject InvokeStatic(TypeID type, std::string_view method, const std::vector<T>& params, bool showError = true) {
            ArgsTypeList list;
            for (auto param : params) {
                list.push_back(param.GetType());
            }
            std::function<void*(void*)> conv = [](void* orig) { return orig; };
            auto* info = SafeGetMethodWithInherit(&conv, type, method, list, showError);
            if (info == nullptr) {
                return SharedObject();
            }
            std::vector<std::shared_ptr<void>> temp;
            auto ret = info->newRet();
            info->getRegister(nullptr, ConvertParams(params, *info, temp), ret);
            return ret;
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
        void AddClass(TagList tagList = {}) {
            auto type = TypeID::get<T>();
            classInfo[type].newObject = [](const std::vector<ObjectPtr>& args) -> SharedObject {
                auto obj = SharedObject{TypeID::get<T>(), std::make_shared<T>(), false};
                obj.ctor(args);
                return obj;
            };
            classInfo[type].tags = tagList;
        }
        void AddAliasClass(std::string_view from, std::string_view to);
        void AddVirtualClass(std::string_view cls, std::function<SharedObject(const std::vector<ObjectPtr>&)> ctor, TagList tagList = {});
        void AddVirtualInheritance(std::string_view cls, std::string_view inherit);
        TagList& GetClassTag(TypeID cls);
        TagList& GetFieldTag(TypeID cls, std::string_view name);
        TagList& GetMethodInfo(TypeID cls, std::string_view name);
        TagList& GetMethodInfo(TypeID cls, std::string_view name, const ArgsTypeList& args);
    public:
        void RawAddField(TypeID cls, std::string_view name, std::function<ObjectPtr(ObjectPtr)> func);
        void RawAddStaticField(TypeID cls, std::string_view name, std::function<ObjectPtr()> func);
        ObjectPtr RawGetField(ObjectPtr instance, std::string_view name);
        void RawAddMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(ObjectPtr, const std::vector<ObjectPtr>&)> func);
        SharedObject RawInvoke(ObjectPtr instance, std::string_view name, const std::vector<ObjectPtr>& params);
        void RawAddStaticMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(const std::vector<ObjectPtr>&)> func);
        SharedObject RawInvokeStatic(TypeID type, std::string_view name, const std::vector<ObjectPtr>& params);
        void SelfExport();
};
