#pragma once
#include <map>
#include <utility>
#include <queue>
#include <functional>
#include "TemplateUtils.h"
#include "Object.h"
#include "TypeID.h"
#include "MetaMethods.h"

using TagList = std::map<std::string, std::vector<std::string>>;

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
    using FuncType = SharedObject(void*, const std::vector<void*>&);
    std::string name;
    TagList tags;
    std::function<FuncType> getRegister;
    TypeID returnType;
    ArgsTypeList argsList;
    MethodInfo& withRegister(std::function<FuncType> getRegister);
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
        std::map<TypeID, std::map<std::string, FieldInfo>> fieldInfo;
        std::map<TypeID, std::map<std::string, std::vector<MethodInfo>>> methodInfo;
        std::map<TypeID, ClassInfo> classInfo;
        template<typename T, typename U>
        auto GetFieldRegisterFunc(T U::* p) {
            return [p](void* instance) {
                return ObjectPtr{ TypeID::get<T>(), (void*)&((U*)(instance)->*p) };
            };
        }
        template<typename T> T* SafeGetList(std::map<TypeID, T>& info, TypeID id);
        template<typename T> T* SafeGet(std::map<TypeID, std::map<std::string, T>>& info, TypeID id, std::string_view name);
        bool CheckParams(MethodInfo info, const ArgsTypeList& list);
        const MethodInfo* SafeGet(TypeID id, std::string_view name, const ArgsTypeList& args);
        template<typename Ret> Ret* WalkThroughInherits(std::function<void*(void*)>* instanceConv, TypeID id, std::function<Ret*(TypeID)> func);
        const FieldInfo* SafeGetFieldWithInherit(std::function<void*(void*)>* instanceConv, TypeID id, std::string_view name, bool showError = true);
        const MethodInfo* SafeGetMethodWithInherit(std::function<void*(void*)>* instanceConv, TypeID id, std::string_view name, const ArgsTypeList& args, bool showError = true);
        bool HasClassInfo(TypeID type);
    public:
        ReflMgr(const ReflMgr&) = delete;
        ReflMgr(ReflMgr&&) = delete;
        ReflMgr(ReflMgr&) = delete;
        static ReflMgr& Instance();
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
            methodInfo[TypeID::get<Type>()][info.name].push_back(                                                               \
                info.withRegister(                                                                                              \
                    GetMethodRegisterFunc(func)                                                                                 \
                )                                                                                                               \
            );                                                                                                                  \
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
            methodInfo[TypeID::get<Type>()][info.name].push_back(
                info.withRegister(GetLambdaRegisterFunc(func))
            );
        }
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, std::function<Ret(Args...)> func, MethodInfo info) {
            info.returnType = TypeID::get<Ret>();
            info.argsList = ArgsTypeList{TypeID::get<Args>()...};
            methodInfo[type][std::string{info.name}].push_back(
                info.withRegister(GetLambdaRegisterStaticFunc(func))
            );
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
            methodInfo[type][std::string{info.name}].push_back(
                info.withRegister(
                    GetStaticMethodRegisterFunc(std::forward<decltype(func)>(func))
                )
            );
        }
        template<typename Ret, typename... Args>
        void AddStaticMethod(TypeID type, Ret (*func)(Args...), std::string_view name) {
            MethodInfo info{ std::string{name} };
            AddStaticMethod(type, func, info);
        }
        std::function<SharedObject(void*, std::vector<void*>)> GetInvokeFunc(TypeID type, std::string_view member, ArgsTypeList list);
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
            std::function<void*(void*)> conv = [](void* orig) { return orig; };
            auto* info = SafeGetMethodWithInherit(&conv, type, method, list, showError);
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
        const TagList& GetClassTag(TypeID cls);
        const TagList& GetFieldTag(TypeID cls, std::string_view name);
        const TagList& GetMethodInfo(TypeID cls, std::string_view name);
        const TagList& GetMethodInfo(TypeID cls, std::string_view name, const ArgsTypeList& args);
    private:
        // functions to be exported
        void ExportAddField(TypeID cls, std::string_view name, std::function<ObjectPtr(ObjectPtr)> func);
        void ExportAddStaticField(TypeID cls, std::string_view name, std::function<ObjectPtr()> func);
        ObjectPtr ExportGetField(ObjectPtr instance, std::string_view name);
        void RawAddMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(ObjectPtr, const std::vector<ObjectPtr>&)> func);
        SharedObject ExportInvoke(ObjectPtr instance, std::string_view name, const std::vector<ObjectPtr>& params);
        void ExportAddStaticMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(const std::vector<ObjectPtr>&)> func);
        SharedObject ExportInvokeStatic(TypeID type, std::string_view name, const std::vector<ObjectPtr>& params);
    public:
        void SelfExport();
};
