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

template<typename T> T* ReflMgr::SafeGetList(std::map<TypeID, T>& info, TypeID id) {
    if (info.find(id) == info.end()) {
        return nullptr;
    }
    return &info[id];
}

template ClassInfo* ReflMgr::SafeGetList(std::map<TypeID, ClassInfo>& info, TypeID id);
template std::map<std::string, std::vector<MethodInfo>>* ReflMgr::SafeGetList(std::map<TypeID, std::map<std::string, std::vector<MethodInfo>>>& info, TypeID id);
template std::map<std::string, FieldInfo>* ReflMgr::SafeGetList(std::map<TypeID, std::map<std::string, FieldInfo>>& info, TypeID id);

template<typename T> T* ReflMgr::SafeGet(std::map<TypeID, std::map<std::string, T>>& info, TypeID id, std::string_view name) {
    auto* lst = SafeGetList(info, id);
    if (lst == nullptr) {
        return nullptr;
    }
    if (lst->find(std::string{name}) == lst->end()) {
        return nullptr;
    }
    return &(*lst)[std::string{name}];
}

template std::vector<MethodInfo>* ReflMgr::SafeGet(std::map<TypeID, std::map<std::string, std::vector<MethodInfo>>>& info, TypeID id, std::string_view name);
template FieldInfo* ReflMgr::SafeGet(std::map<TypeID, std::map<std::string, FieldInfo>>& info, TypeID id, std::string_view name);

bool ReflMgr::CheckParams(MethodInfo info, const ArgsTypeList& lst) {
    if (info.argsList.size() != lst.size()) {
        return false;
    }
    for (int i = 0; i < info.argsList.size(); i++) {
        if (!lst[i].canBeAppliedTo(info.argsList[i])) {
            return false;
        }
    }
    return true;
}

const MethodInfo* ReflMgr::SafeGet(TypeID id, std::string_view name, const ArgsTypeList& args) {
    auto* lst = SafeGet(methodInfo, id, name);
    if (lst == nullptr) {
        return nullptr;
    }
    for (const auto& info : *lst) {
        if (CheckParams(info, args)) {
            return &info;
        }
    }
    return nullptr;
}

template<typename Ret> Ret* ReflMgr::WalkThroughInherits(std::function<void*(void*)>* conv, TypeID id, std::function<Ret*(TypeID)> func) {
    std::queue<std::pair<TypeID, std::function<void*(void*)>>> q;
    q.push({id, *conv});
    while (!q.empty()) {
        auto [type, convFunc] = q.front();
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

template FieldInfo* ReflMgr::WalkThroughInherits(std::function<void*(void*)>* conv, TypeID id, std::function<FieldInfo*(TypeID)> func);
template MethodInfo* ReflMgr::WalkThroughInherits(std::function<void*(void*)>* conv, TypeID id, std::function<MethodInfo*(TypeID)> func);

const FieldInfo* ReflMgr::SafeGetFieldWithInherit(std::function<void*(void*)>* conv, TypeID id, std::string_view name, bool showError) {
    auto ret = WalkThroughInherits(conv, id, std::function([&](TypeID type) { return SafeGet(fieldInfo, type, name); }));
    if (ret == nullptr && showError) {
        std::cerr << "Error: no matching field found: " << id.getName() << "::" << name << std::endl;
    }
    return ret;
}

const MethodInfo* ReflMgr::SafeGetMethodWithInherit(std::function<void*(void*)>* conv, TypeID id, std::string_view name, const ArgsTypeList& args, bool showError) {
    auto ret = WalkThroughInherits(conv, id, std::function([&](TypeID type) { return SafeGet(type, name, args); }));
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

bool ReflMgr::HasClassInfo(TypeID type) {
    return classInfo.find(type) != classInfo.end();
}

SharedObject ReflMgr::New(TypeID type, const std::vector<ObjectPtr>& args) {
    TypeID target = type;
    while (!classInfo[target].aliasTo.isNull()) {
        target = classInfo[target].aliasTo;
    }
    auto& func = classInfo[target].newObject;
    if (func == 0) {
        std::cerr << "Error: unable to init an unregistered class: " << type.getName() << "(aliased to " << target.getName() << " )" << std::endl;
        return SharedObject::Null;
    }
    return func(args);
}

SharedObject ReflMgr::New(std::string_view typeName, const std::vector<ObjectPtr>& args) {
    return New(TypeID::getRaw(typeName), args);
}

ObjectPtr ReflMgr::RawGetField(TypeID type, void* instance, std::string_view member) {
    std::function<void*(void*)> conv = [](void* orig) { return orig; };
    auto* p = SafeGetFieldWithInherit(&conv, type, member);
    if (p == nullptr) {
        return ObjectPtr::Null;
    }
    return p->getRegister(conv(instance));
}

std::function<SharedObject(void*, std::vector<void*>)> ReflMgr::GetInvokeFunc(TypeID type, std::string_view member, ArgsTypeList list) {
    std::function<void*(void*)> conv = [](void* orig) { return orig; };
    auto* info = SafeGetMethodWithInherit(&conv, type, member, list);
    if (info == nullptr || info->name == "") {
        return nullptr;
    }
    return [info, conv](void* instance, std::vector<void*> params) {
        return info->getRegister(conv(instance), params);
    };
}

SharedObject ReflMgr::RawInvoke(TypeID type, void* instance, std::string_view member, ArgsTypeList list, std::vector<void*> params) {
    std::function<void*(void*)> conv = [](void* orig) { return orig; };
    auto* info = SafeGetMethodWithInherit(&conv, type, member, list);
    if (info == nullptr || info->name == "") {
        return SharedObject::Null;
    }
    return info->getRegister(conv(instance), params);
}

void ReflMgr::AddAliasClass(std::string_view from, std::string_view to) {
    classInfo[TypeID::getRaw(from)].aliasTo = TypeID::getRaw(to);
}

void ReflMgr::AddVirtualClass(std::string_view cls, std::function<SharedObject(const std::vector<ObjectPtr>&)> ctor, TagList tagList) {
    auto type = TypeID::getRaw(cls);
    classInfo[type].newObject = ctor;
    classInfo[type].tags = tagList;
}

const TagList& ReflMgr::GetClassTag(TypeID cls) {
    return classInfo[cls].tags;
}
const TagList& ReflMgr::GetFieldTag(TypeID cls, std::string_view name) {
    return fieldInfo[cls][std::string{name}].tags;
}
const TagList& ReflMgr::GetMethodInfo(TypeID cls, std::string_view name) {
    return methodInfo[cls][std::string{name}][0].tags;
}
const TagList& ReflMgr::GetMethodInfo(TypeID cls, std::string_view name, const ArgsTypeList& args) {
    auto& infos = methodInfo[cls][std::string{name}];
    for (auto& info : infos) {
        if (CheckParams(info, args)) {
            return info.tags;
        }
    }
    return infos[0].tags;
}

void ReflMgr::ExportAddField(TypeID cls, std::string_view name, std::function<ObjectPtr(ObjectPtr)> func) {
    FieldInfo info{ std::string{name} };
    fieldInfo[cls][std::string{name}] = info.withRegister([func, cls](void* ptr) { return func(ObjectPtr{cls, ptr}); });
}

void ReflMgr::ExportAddStaticField(TypeID cls, std::string_view name, std::function<ObjectPtr()> func) {
    FieldInfo info{ std::string{name} };
    fieldInfo[cls][std::string{name}] = info.withRegister([func](void* ptr) { return func(); });
}

ObjectPtr ReflMgr::ExportGetField(ObjectPtr instance, std::string_view name) {
    return RawGetField(instance.GetType(), instance.GetRawPtr(), name);
}

void ReflMgr::RawAddMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(ObjectPtr, const std::vector<ObjectPtr>&)> func) {
    MethodInfo info{ std::string{name} };
    info.returnType = returnType;
    info.argsList = argsList;
    methodInfo[cls][std::string{name}].push_back(info.withRegister([func, argsList, cls](void* instance, const std::vector<void*>& params) -> SharedObject {
        std::vector<ObjectPtr> args;
        for (int i = 0; i < params.size(); i++) {
            args.push_back(ObjectPtr{argsList[i], params[i]});
        }
        return func(ObjectPtr{cls, instance}, args);
    }));
}

SharedObject ReflMgr::ExportInvoke(ObjectPtr instance, std::string_view name, const std::vector<ObjectPtr>& params) {
    ArgsTypeList list;
    std::vector<void*> args;
    for (auto param : params) {
        args.push_back(param.GetRawPtr());
        list.push_back(param.GetType());
    }
    return RawInvoke(instance.GetType(), instance.GetRawPtr(), name, list, args);
}

void ReflMgr::ExportAddStaticMethod(TypeID cls, std::string_view name, TypeID returnType, const ArgsTypeList& argsList, std::function<SharedObject(const std::vector<ObjectPtr>&)> func) {
    MethodInfo info{ std::string{name} };
    info.returnType = returnType;
    info.argsList = argsList;
    methodInfo[cls][std::string{name}].push_back(info.withRegister([func, argsList](void* instance, const std::vector<void*>& params) -> SharedObject {
        std::vector<ObjectPtr> args;
        for (int i = 0; i < params.size(); i++) {
            args.push_back(ObjectPtr{argsList[i], params[i]});
        }
        return func(args);
    }));
}

SharedObject ReflMgr::ExportInvokeStatic(TypeID type, std::string_view name, const std::vector<ObjectPtr>& params) {
    return InvokeStatic(type, name, params);
}

void ReflMgr::SelfExport() {
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
