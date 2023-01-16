#include "ReflMgr.h"

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

bool ReflMgr::CheckParams(const MethodInfo& info, const ArgsTypeList& lst, const ArgsTypeList& classTemplateArgs, const ArgsTypeList& funcTemplateArgs, int checkMode) {
    if (info.argsList.size() != lst.size()) {
        return false;
    }
    for (int i = 0; i < info.argsList.size(); i++) {
        if (!info.argsList[i].Match(classTemplateArgs, funcTemplateArgs, lst[i], checkMode)) {
            return false;
        }
    }
    return true;
}

const MethodInfo* ReflMgr::SafeGet(TemplatedTypeID id, std::string_view name, const ArgsTypeList& args, const ArgsTypeList& funcTemplateArgs) {
    auto* lst = SafeGet(methodInfo, id.base, name);
    if (lst == nullptr) {
        return nullptr;
    }
    for (const auto& info : *lst) {
        if (CheckParams(info, args, id.args, funcTemplateArgs)) {
            return &info;
        }
    }
    return nullptr;
}

template<typename Ret> Ret* ReflMgr::WalkThroughInherits(void** instance, TemplatedTypeID id, std::function<Ret*(TemplatedTypeID)> func) {
    std::queue<std::pair<TemplatedTypeID, void*>> q;
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
        if (HasClassInfo(type.base)) {
            ClassInfo& info = classInfo[type.base];
            for (int i = 0; i < info.get().parents.size(); i++) {
                q.push({info.get().parents[i], instance == nullptr ? nullptr : info.get().cast[i](ptr)});
            }
        }
    }
    return nullptr;
}

template FieldInfo* ReflMgr::WalkThroughInherits(void** instance, TemplatedTypeID id, std::function<FieldInfo*(TemplatedTypeID)> func);
template MethodInfo* ReflMgr::WalkThroughInherits(void** instance, TemplatedTypeID id, std::function<MethodInfo*(TemplatedTypeID)> func);

const FieldInfo* ReflMgr::SafeGetFieldWithInherit(void** instance, TemplatedTypeID id, std::string_view name, bool showError) {
    auto ret = WalkThroughInherits(instance, id, std::function([&](TemplatedTypeID type) { return SafeGet(fieldInfo, type.base, name); }));
    if (ret == nullptr && showError) {
        std::cerr << "Error: no matching field found: " << id.getName() << "::" << name << std::endl;
    }
    return ret;
}

const MethodInfo* ReflMgr::SafeGetMethodWithInherit(void** instance, TemplatedTypeID id, std::string_view name, const ArgsTypeList& args, const ArgsTypeList& funcTemplateArgs, bool showError) {
    auto ret = WalkThroughInherits(instance, id, std::function([&](TemplatedTypeID type) { return SafeGet(type, name, args, funcTemplateArgs); }));
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

SharedObject ReflMgr::New(TypeID type, const std::vector<ObjectPtr>& args, const ArgsTypeList& templateArgs) {
    auto& func = classInfo[type].get().newObject;
    if (func == 0) {
        std::cerr << "Error: unable to init an unregistered class: " << type.getName() << std::endl;
        return SharedObject::Null;
    }
    return func(args).AddTemplateArgs(templateArgs);
}

SharedObject ReflMgr::New(std::string_view typeName, const std::vector<ObjectPtr>& args, const ArgsTypeList& templateArgs) {
    return New(TypeID::getRaw(typeName), args, templateArgs);
}

ObjectPtr ReflMgr::RawGetField(TemplatedTypeID type, void* instance, std::string_view member) {
    auto* p = SafeGetFieldWithInherit(&instance, type, member);
    if (p == nullptr) {
        return ObjectPtr::Null;
    }
    return p->getRegister(instance, type.args);
}

SharedObject ReflMgr::RawInvoke(TemplatedTypeID type, void* instance, std::string_view member, const ArgsTypeList& list, const ArgsTypeList& funcTemplateArgs, std::vector<void*> params) {
    auto* info = SafeGetMethodWithInherit(&instance, type, member, list, funcTemplateArgs);
    if (info == nullptr || info->name == "") {
        return SharedObject::Null;
    }
    return info->getRegister(instance, params, type.args, funcTemplateArgs);
}

void ReflMgr::AddVirtualClass(std::string_view cls, std::function<SharedObject(const std::vector<ObjectPtr>&)> ctor, TagList tagList) {
    auto type = TypeID::getRaw(cls);
    classInfo[type].get().newObject = ctor;
    classInfo[type].get().tags = tagList;
}

bool ReflMgr::SameClass(TypeID a, TypeID b) {
    return &classInfo[a].get() == &classInfo[b].get();
}

void ReflMgr::AliasClass(std::string_view alias, TypeID target) {
    classInfo[TypeID::getRaw(alias)].aliasTo = &classInfo[target];
}

const TagList& ReflMgr::GetClassTag(TypeID cls) {
    return classInfo[cls].get().tags;
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
        if (CheckParams(info, args, {}, {}, 3)) {
            return info.tags;
        }
    }
    return infos[0].tags;
}

void ReflMgr::ExportAddField(TemplatedTypeID cls, std::string_view name, std::function<ObjectPtr(ObjectPtr)> func) {
    FieldInfo info{ std::string{name} };
    fieldInfo[cls.base][std::string{name}] = info.withRegister([func, cls](void* ptr, const ArgsTypeList&) { return func(ObjectPtr{cls, ptr}); });
}

void ReflMgr::ExportAddStaticField(TemplatedTypeID cls, std::string_view name, std::function<ObjectPtr()> func) {
    FieldInfo info{ std::string{name} };
    fieldInfo[cls.base][std::string{name}] = info.withRegister([func](void* ptr, const ArgsTypeList&) { return func(); });
}

ObjectPtr ReflMgr::ExportGetField(ObjectPtr instance, std::string_view name) {
    return RawGetField(instance.GetType().base, instance.GetRawPtr(), name);
}

void ReflMgr::RawAddMethod(TypeID cls, std::string_view name, IncompleteType returnType, const std::vector<IncompleteType>& argsList, std::function<SharedObject(ObjectPtr, const std::vector<ObjectPtr>&)> func) {
    MethodInfo info{ std::string{name} };
    info.returnType = returnType;
    info.argsList = argsList;
    methodInfo[cls][std::string{name}].push_back(info.withRegister([func, cls, returnType, argsList](void* instance, const std::vector<void*>& params, const ArgsTypeList& classTemplateArgs, const ArgsTypeList& funcTemplateArgs) -> SharedObject {
        std::vector<ObjectPtr> args;
        for (int i = 0; i < params.size(); i++) {
            args.push_back(ObjectPtr{argsList[i].ApplyWith(classTemplateArgs, funcTemplateArgs), params[i]});
        }
        return func(ObjectPtr{cls, instance}, args);
    }));
}

SharedObject ReflMgr::ExportInvoke(ObjectPtr instance, std::string_view name, const std::vector<ObjectPtr>& params, const ArgsTypeList& funcTemplateArgs) {
    ArgsTypeList list;
    std::vector<void*> args;
    for (auto param : params) {
        args.push_back(param.GetRawPtr());
        list.push_back(param.GetType());
    }
    return RawInvoke(instance.GetType(), instance.GetRawPtr(), name, list, funcTemplateArgs, args);
}

void ReflMgr::ExportAddStaticMethod(TemplatedTypeID cls, std::string_view name, IncompleteType returnType, const std::vector<IncompleteType>& argsList, std::function<SharedObject(const std::vector<ObjectPtr>&)> func) {
    MethodInfo info{ std::string{name} };
    info.returnType = returnType;
    info.argsList = argsList;
    methodInfo[cls.base][std::string{name}].push_back(info.withRegister([func, argsList](void* instance, const std::vector<void*>& params, const ArgsTypeList& classTemplateArgs, const ArgsTypeList& funcTemplateArgs) -> SharedObject {
        std::vector<ObjectPtr> args;
        for (int i = 0; i < params.size(); i++) {
            args.push_back(ObjectPtr{argsList[i].ApplyWith(classTemplateArgs, funcTemplateArgs), params[i]});
        }
        return func(args);
    }));
}

SharedObject ReflMgr::ExportInvokeStatic(TemplatedTypeID type, std::string_view name, const std::vector<ObjectPtr>& params) {
    return InvokeStatic(type.base, name, params);
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
