#include "ReflMgr.h"

FieldInfo& FieldInfo::withRegister(std::function<ObjectPtr(void*)> getRegister) {
    this->getRegister = getRegister;
    return *this;
}

MethodInfo& MethodInfo::withRegister(std::function<FuncType> getRegister) {
    this->getRegister = getRegister;
    return *this;
}

ReflMgr& ReflMgr::Instance() {
    static ReflMgr instance;
    return instance;
}

#define ERROR std::cerr << errorMsgPrefix
void ReflMgr::SetErrorMsgPrefix(const std::string& msg) {
    errorMsgPrefix = msg;
}

TypeID ReflMgr::GetType(std::string_view clsName) {
    auto& instance = ReflMgr::Instance();
    TypeID target = TypeID::getRaw(clsName);
    if (!instance.HasClassInfo(target)) {
        return target;
    }
    while (!instance.classInfo[target].aliasTo.isNull()) {
        target = instance.classInfo[target].aliasTo;
    }
    return target;
}

template<typename T> T* ReflMgr::SafeGetList(TypeIDMap<T>& info, TypeID id) {
    if (info.find(id) == info.end()) {
        return nullptr;
    }
    return &info[id];
}

template ClassInfo* ReflMgr::SafeGetList(TypeIDMap<ClassInfo>& info, TypeID id);
template std::unordered_map<std::string, std::vector<MethodInfo>>* ReflMgr::SafeGetList(TypeIDMap<std::unordered_map<std::string, std::vector<MethodInfo>>>& info, TypeID id);
template std::unordered_map<std::string, FieldInfo>* ReflMgr::SafeGetList(TypeIDMap<std::unordered_map<std::string, FieldInfo>>& info, TypeID id);

template<typename T> T* ReflMgr::SafeGet(TypeIDMap<std::unordered_map<std::string, T>>& info, TypeID id, std::string_view name) {
    auto* lst = SafeGetList(info, id);
    if (lst == nullptr) {
        return nullptr;
    }
    if (lst->find(std::string{name}) == lst->end()) {
        return nullptr;
    }
    return &(*lst)[std::string{name}];
}

template std::vector<MethodInfo>* ReflMgr::SafeGet(TypeIDMap<std::unordered_map<std::string, std::vector<MethodInfo>>>& info, TypeID id, std::string_view name);
template FieldInfo* ReflMgr::SafeGet(TypeIDMap<std::unordered_map<std::string, FieldInfo>>& info, TypeID id, std::string_view name);

template<typename MethodInfoType>
void ReflMgr::CheckParams(MethodInfoType& info, const ArgsTypeList& lst, MethodInfoType** rec) {
    if (info.argsList.size() != lst.size()) {
        return;
    }
    bool same = true;
    bool generics = false;
    for (int i = 0; i < info.argsList.size(); i++) {
        if (lst[i].getHash() != info.argsList[i].getHash()) {
            same = false;
        }
        bool canUseGenerics = info.argsList[i].getHash() == TypeID::get<ReflMgr::Any>().getHash();
        if (canUseGenerics) {
            generics = true;
        }
        if (!lst[i].canBeAppliedTo(info.argsList[i]) && !canUseGenerics) {
            std::cout << lst[i].getName() << " cannot apply to " << info.argsList[i].getName() << std::endl;
            return;
        }
    }
    if (same && !generics) {
        rec[0] = &info;
    } else if (!generics) {
        rec[1] = &info;
    } else {
        rec[2] = &info;
    }
}

template void ReflMgr::CheckParams(MethodInfo& info, const ArgsTypeList& lst, MethodInfo** rec);
template void ReflMgr::CheckParams(MethodInfo const& info, const ArgsTypeList& lst, MethodInfo const** rec);

const MethodInfo* ReflMgr::SafeGet(TypeID id, std::string_view name, const ArgsTypeList& args) {
    auto* lst = SafeGet(methodInfo, id, name);
    if (lst == nullptr) {
        return nullptr;
    }
    const MethodInfo* rec[3] = { nullptr, nullptr, nullptr };
    for (const auto& info : *lst) {
        CheckParams(info, args, rec);
    }
    for (int i = 0; i < 3; i++) {
        if (rec[i] != nullptr) {
            return rec[i];
        }
    }
    return nullptr;
}

template<typename Ret> Ret* ReflMgr::WalkThroughInherits(std::function<void*(void*)>* conv, TypeID id, std::function<Ret*(TypeID)> func) {
    std::queue<std::pair<TypeID, std::function<void*(void*)>>> q;
    q.push({id, *conv});
    while (!q.empty()) {
        auto type = q.front().first;
        auto convFunc = q.front().second;
        q.pop();
        auto* ret = func(type);
        if (ret != nullptr) {
            *conv = convFunc;
            return ret;
        }
        if (HasClassInfo(type)) {
            ClassInfo& info = classInfo[type];
            for (int i = 0; i < info.parents.size(); i++) {
                q.push({info.parents[i], [convFunc, info, i](void* instance) { return info.cast[i](convFunc(instance)); }});
            }
        }
    }
    return nullptr;
}

bool MethodInfo::sameDeclareTo(const MethodInfo& other) const {
    if (returnType != other.returnType || argsList.size() != other.argsList.size()) {
        return false;
    }
    for (int i = 0; i < argsList.size(); i++) {
        if (argsList[i] != other.argsList[i]) {
            return false;
        }
    }
    return true;
}

void ReflMgr::AddMethodInfo(TypeID type, const std::string& name, MethodInfo info, bool overridePrevious) {
    std::vector<MethodInfo>& lst = methodInfo[type][name];
    for (MethodInfo& data : lst) {
        if (data.sameDeclareTo(info)) {
            if (overridePrevious) {
                data = info;
            } else {
                ERROR << "Error: same type of function already registered: " << info.returnType.getName() << " " << type.getName() << "::" << name << "(";
                if (info.argsList.size() > 0) {
                    ERROR << info.argsList[0].getName();
                }
                for (int i = 1; i < info.argsList.size(); i++) {
                    ERROR << ", " << info.argsList[i].getName();
                }
                ERROR << ")" << std::endl;
            }
        }
    }
    methodInfo[type][name].push_back(info);
}

template FieldInfo* ReflMgr::WalkThroughInherits(std::function<void*(void*)>* conv, TypeID id, std::function<FieldInfo*(TypeID)> func);
template MethodInfo* ReflMgr::WalkThroughInherits(std::function<void*(void*)>* conv, TypeID id, std::function<MethodInfo*(TypeID)> func);

const FieldInfo* ReflMgr::SafeGetFieldWithInherit(std::function<void*(void*)>* conv, TypeID id, std::string_view name, bool showError) {
    auto ret = WalkThroughInherits(conv, id, std::function([&](TypeID type) { return SafeGet(fieldInfo, type, name); }));
    if (ret == nullptr && showError) {
        ERROR << "Error: no matching field found: " << id.getName() << "::" << name << std::endl;
    }
    return ret;
}

const MethodInfo* ReflMgr::SafeGetMethodWithInherit(std::function<void*(void*)>* conv, TypeID id, std::string_view name, const ArgsTypeList& args, bool showError) {
    auto ret = WalkThroughInherits(conv, id, std::function([&](TypeID type) { return SafeGet(type, name, args); }));
    if (ret == nullptr && showError) {
        ERROR << "Error: no matching method found: " << id.getName() << "::" << name << "(";
        if (args.size() > 0) {
            ERROR << args[0].getName();
            for (int i = 1; i < args.size(); i++) {
                ERROR << ", " << args[i].getName();
            }
        }
        ERROR << ")" << std::endl;
    }
    return ret;
}

bool ReflMgr::HasClassInfo(TypeID type) {
    return classInfo.find(type) != classInfo.end();
}

static inline std::string_view removeNameRefAndConst(std::string_view name) {
    int start = 0, end = name.length();
    if (name.length() > 6 && name.substr(0, 6) == "const ") {
        start += 6;
    }
    if (name[name.length() - 1] == '&') {
        end--;
    }
    return name.substr(start, std::max(0, end - start));
}

SharedObject ReflMgr::New(TypeID type, const std::vector<ObjectPtr>& args) {
    auto& func = classInfo[TypeID::getRaw(removeNameRefAndConst(type.getName()))].newObject;
    if (func == 0) {
        ERROR << "Error: unable to init an unregistered class: " << type.getName() << std::endl;
        return SharedObject::Null;
    }
    return func(args);
}

SharedObject ReflMgr::New(std::string_view typeName, const std::vector<ObjectPtr>& args) {
    return New(ReflMgr::GetType(typeName), args);
}

ObjectPtr ReflMgr::RawGetField(TypeID type, void* instance, std::string_view member) {
    std::function<void*(void*)> conv = [](void* orig) { return orig; };
    auto* p = SafeGetFieldWithInherit(&conv, type, member);
    if (p == nullptr) {
        return ObjectPtr::Null;
    }
    return p->getRegister(conv(instance));
}

std::function<void(void*, std::vector<void*>, SharedObject& ret)> ReflMgr::GetInvokeFunc(TypeID type, std::string_view member, ArgsTypeList list) {
    std::function<void*(void*)> conv = [](void* orig) { return orig; };
    auto* info = SafeGetMethodWithInherit(&conv, type, member, list);
    if (info == nullptr || info->name == "") {
        return nullptr;
    }
    return [info, conv](void* instance, std::vector<void*> params, SharedObject& ret) {
        info->getRegister(conv(instance), params, ret);
    };
}

SharedObject ReflMgr::RawInvoke(TypeID type, void* instance, std::string_view member, ArgsTypeList list, std::vector<void*> params) {
    std::function<void*(void*)> conv = [](void* orig) { return orig; };
    auto* info = SafeGetMethodWithInherit(&conv, type, member, list);
    if (info == nullptr || info->name == "") {
        return SharedObject::Null;
    }
    auto ret = info->newRet();
    info->getRegister(conv(instance), params, ret);
    return ret;
}

void ReflMgr::AddAliasClass(std::string_view from, std::string_view to) {
    classInfo[TypeID::getRaw(from)].aliasTo = TypeID::getRaw(to);
}

void ReflMgr::AddVirtualClass(std::string_view cls, std::function<SharedObject(const std::vector<ObjectPtr>&)> ctor, TagList tagList) {
    auto type = TypeID::getRaw(cls);
    classInfo[type].newObject = ctor;
    classInfo[type].tags = tagList;
}

void ReflMgr::AddVirtualInheritance(std::string_view cls, std::string_view inherit) {
    ClassInfo& info = classInfo[TypeID::getRaw(cls)];
    info.parents.push_back(TypeID::getRaw(inherit));
    info.cast.push_back([](void* derived) -> void* {
        return derived;
    });
}

static inline TagList nullTag;

TagList& ReflMgr::GetClassTag(TypeID cls) {
    return classInfo[cls].tags;
}
TagList& ReflMgr::GetFieldTag(TypeID cls, std::string_view name) {
    return fieldInfo[cls][std::string{name}].tags;
}
TagList& ReflMgr::GetMethodInfo(TypeID cls, std::string_view name) {
    return methodInfo[cls][std::string{name}][0].tags;
}
TagList& ReflMgr::GetMethodInfo(TypeID cls, std::string_view name, const ArgsTypeList& args) {
    auto& infos = methodInfo[cls][std::string{name}];
    MethodInfo* rec[3] = { nullptr, nullptr, nullptr };
    for (auto& info : infos) {
        CheckParams(info, args, rec);
    }
    for (int i = 0; i < 3; i++) {
        if (rec[i] != nullptr) {
            return rec[i]->tags;
        }
    }
    return nullTag;
}

void ReflMgr::RawAddField(TypeID cls, std::string_view name, std::function<ObjectPtr(ObjectPtr)> func) {
    FieldInfo info{ std::string{name} };
    fieldInfo[cls][std::string{name}] = info.withRegister([func, cls](void* ptr) { return func(ObjectPtr{cls, ptr}); });
}

void ReflMgr::RawAddStaticField(TypeID cls, std::string_view name, std::function<ObjectPtr()> func) {
    FieldInfo info{ std::string{name} };
    fieldInfo[cls][std::string{name}] = info.withRegister([func](void* ptr) { return func(); });
}

ObjectPtr ReflMgr::RawGetField(ObjectPtr instance, std::string_view name) {
    return RawGetField(instance.GetType(), instance.GetRawPtr(), name);
}

void ReflMgr::RawAddMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(ObjectPtr, const std::vector<ObjectPtr>&)> func) {
    MethodInfo info{ std::string{name} };
    info.returnType = returnType;
    info.argsList = argsList;
    info.newRet = []() { return SharedObject(); };
    AddMethodInfo(cls, info.name, info.withRegister([func, argsList, cls](void* instance, const std::vector<void*>& params, SharedObject& ret) {
        std::vector<ObjectPtr> args;
        for (int i = 0; i < params.size(); i++) {
            args.push_back(ObjectPtr{argsList[i], params[i]});
        }
        ret = func(ObjectPtr{cls, instance}, args);
    }));
}

SharedObject ReflMgr::RawInvoke(ObjectPtr instance, std::string_view name, const std::vector<ObjectPtr>& params) {
    ArgsTypeList list;
    std::vector<void*> args;
    for (auto param : params) {
        args.push_back(param.GetRawPtr());
        list.push_back(param.GetType());
    }
    return RawInvoke(instance.GetType(), instance.GetRawPtr(), name, list, args);
}

void ReflMgr::RawAddStaticMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(const std::vector<ObjectPtr>&)> func) {
    MethodInfo info{ std::string{name} };
    info.returnType = returnType;
    info.argsList = argsList;
    info.newRet = []() { return SharedObject(); };
    AddMethodInfo(cls, info.name, info.withRegister([func, argsList](void* instance, const std::vector<void*>& params, SharedObject& ret) {
        std::vector<ObjectPtr> args;
        for (int i = 0; i < params.size(); i++) {
            args.push_back(ObjectPtr{argsList[i], params[i]});
        }
        ret = func(args);
    }));
}

SharedObject ReflMgr::RawInvokeStatic(TypeID type, std::string_view name, const std::vector<ObjectPtr>& params) {
    return InvokeStatic(type, name, params);
}

void ReflMgr::SelfExport() {
    auto& mgr = Instance();
    mgr.AddStaticMethod(TypeID::get<ReflMgr>(), &ReflMgr::Instance, "Instance");
    mgr.AddMethod(&ReflMgr::RawAddField, "AddField");
    mgr.AddMethod(&ReflMgr::RawAddStaticField, "AddStaticField");
    mgr.AddMethod(MethodType<ObjectPtr, ReflMgr, ObjectPtr, std::string_view>::Type(&ReflMgr::RawGetField), "GetField");
    mgr.AddMethod(&ReflMgr::RawAddMethod, "AddMethod");
    mgr.AddMethod(MethodType<SharedObject, ReflMgr, ObjectPtr, std::string_view, const std::vector<ObjectPtr>&>::Type(&ReflMgr::RawInvoke), "Invoke");
    mgr.AddMethod(&ReflMgr::RawAddStaticMethod, "AddStaticMethod");
    mgr.AddMethod(&ReflMgr::RawInvokeStatic, "InvokeStatic");
}
