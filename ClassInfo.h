#pragma once
#include <map>
#include <vector>
#include <string>
#include <variant>
#include "Object.h"

using TagList = std::map<std::string, std::vector<std::string>>;

struct IncompleteType {
    IncompleteType();
    IncompleteType(TypeID type);
    IncompleteType(int argID);
    IncompleteType(TypeID type, const std::vector<IncompleteType>& params);
    TypeID type;
    int argID = 0;
    std::vector<IncompleteType> params;
    // 0: 只检查哈希，忽略修饰符
    // 1: 检查哈希和修饰符
    // 2：检查能否隐式转换
    // 3: 默认模板完全匹配
    bool Match(const ArgsTypeList& classTArgs, const ArgsTypeList& funcTArgs, const TemplatedTypeID& other, int checkMode = 0) const;
    TemplatedTypeID ApplyWith(const ArgsTypeList& classTArgs, const ArgsTypeList& funcTArgs) const;
};

struct FieldInfo {
    std::string name;
    TagList tags;
    std::function<ObjectPtr(void*, const ArgsTypeList&)> getRegister;
    FieldInfo& withRegister(std::function<ObjectPtr(void*, const ArgsTypeList&)> getRegister);
};

struct MethodInfo {
    using FuncType = SharedObject(void*, const std::vector<void*>&, const ArgsTypeList&, const ArgsTypeList&);
    std::string name;
    TagList tags;
    std::function<FuncType> getRegister;
    IncompleteType returnType;
    std::vector<IncompleteType> argsList;
    MethodInfo& withRegister(std::function<FuncType> getRegister);
};

template<typename T>
struct FieldType {
    T type;
    FieldInfo info;
    FieldType(T type, FieldInfo info) : type(type), info(info) {}
    FieldType(T type, std::string_view name) : type(type), info({std::string{name}}) {}
};

struct BaseClassInfo {
    TagList tags;
    std::vector<TypeID> parents;
    std::vector<std::function<void*(void*)>> cast;
    std::function<SharedObject(const std::vector<ObjectPtr>&)> newObject = 0;
};

class ClassInfo: private BaseClassInfo {
    public:
        ClassInfo* aliasTo = nullptr;
        BaseClassInfo& get();
};
