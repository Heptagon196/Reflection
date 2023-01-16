#include "ClassInfo.h"

FieldInfo& FieldInfo::withRegister(std::function<ObjectPtr(void*, const ArgsTypeList&)> getRegister) {
    this->getRegister = getRegister;
    return *this;
}

MethodInfo& MethodInfo::withRegister(std::function<FuncType> getRegister) {
    this->getRegister = getRegister;
    return *this;
}

BaseClassInfo& ClassInfo::get() {
    ClassInfo* ptr = this;
    while (ptr->aliasTo != nullptr) {
        ptr = ptr->aliasTo;
    }
    return *ptr;
}

IncompleteType::IncompleteType() {}
IncompleteType::IncompleteType(TypeID type) : type(type) {}
IncompleteType::IncompleteType(int argID) : argID(argID) {}
IncompleteType::IncompleteType(TypeID type, const std::vector<IncompleteType>& params) : type(type), params(params) {}

static inline bool Check(TypeID a, TypeID b, int checkMode) {
    if (checkMode == 0) {
        return a.equalTo(b, true);
    } else if (checkMode == 1) {
        return a == b;
    } else if (checkMode == 2) {
        return b.canBeAppliedTo(a);
    }
    return true;
}

bool IncompleteType::Match(const ArgsTypeList& classTArgs, const ArgsTypeList& funcTArgs, const TemplatedTypeID& other, int checkMode) const {
    if (params.size() != other.args.size()) {
        return false;
    }
    if (!(((argID == 0 && Check(type, other.base, checkMode)))
       || (argID != 0 && (checkMode == 3 || Check(argID > 0 ? classTArgs[argID - 1].base : funcTArgs[-(argID + 1)].base, other.base, checkMode))))) {
        return false;
    }
    for (int i = 0; i < params.size(); i++) {
        if (!params[i].Match(classTArgs, funcTArgs, other.args[i], checkMode)) {
            return false;
        }
    }
    return true;
}

TemplatedTypeID IncompleteType::ApplyWith(const ArgsTypeList& classTArgs, const ArgsTypeList& funcTArgs) const {
    TemplatedTypeID ret;
    if (argID == 0) {
        ret.base = type;
    } else {
        ret.base = argID > 0 ? classTArgs[argID - 1].base : funcTArgs[-(argID + 1)].base;
    }
    for (int i = 0; i < params.size(); i++) {
        ret.args.push_back(params[i].ApplyWith(classTArgs, funcTArgs));
    }
    return ret;
}
