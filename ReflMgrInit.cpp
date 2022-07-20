#include <cmath>
#include <sstream>
#include <cstdarg>
#include <map>
#include "ReflMgrInit.h"

template<typename T>
void print(std::stringstream& out, const T& val) {
    if constexpr (std::same_as<T, std::string> || std::same_as<T, std::string_view>) {
        out << '"' << val << '"';
    } else {
        if constexpr (std::same_as<T, SharedObject> || std::same_as<T, ObjectPtr>) {
            if (val.GetType() == TypeID::get<std::string>() || val.GetType() == TypeID::get<std::string_view>()) {
                out << '"' << val << '"';
            } else {
                out << val;
            }
        } else {
            out << val;
        }
    }
}

template<typename T>
void printVec(std::stringstream& out, const T& val) {
    out << "[";
    if (val.size() > 0) {
        out << " ";
        print(out, val[0]);
        for (int i = 1; i < val.size(); i++) {
            out << ", ";
            print(out, val[i]);
        }
    }
    out << " ]";
}

static bool useIndent = true;
static int indentWidth = 2;
static int indent = 0;

void printIndent(std::stringstream& out) {
    for (int i = 0; i < indent * indentWidth; i++) {
        out << " ";
    }
}

template<typename T, typename V>
void printMap(std::stringstream& out, const std::map<T, V>& val) {
    out << "{";
    if (useIndent) {
        indent++;
    }
    if (val.size() > 0) {
        auto iter = val.begin();
        if (useIndent) {
            out << std::endl;
            printIndent(out);
        } else {
            out << " ";
        }
        print(out, iter->first);
        out << ": ";
        print(out, iter->second);
        iter++;
        while (iter != val.end()) {
            out << ",";
            if (useIndent) {
                out << std::endl;
                printIndent(out);
            } else {
                out << " ";
            }
            print(out, iter->first);
            out << ": ";
            print(out, iter->second);
            iter++;
        }
    } else {
        out << " ";
    }
    if (useIndent) {
        indent--;
        out << std::endl;
        printIndent(out);
    }
    out << "}";
}

namespace ReflMgrTool {
    void InitBaseTypes() {
        auto& mgr = ReflMgr::Instance();
#define DEFTYPE(type)                                           \
        mgr.AddClass<type>();                                   \
        ReflMgrTool::AutoRegister<type>();                      \
        DEFCTOR(type)                                           \

#define DEFSINGLE_BASE(type, op, meta) DEFSINGLE(type, { return op; }, meta)

#define DEFCTOR(type) DEFDOUBLE(type, { *self = other; return; }, ctor)

#define DEFDOUBLE(type, func, meta)                                                     \
        mgr.AddMethod<type>(                                                            \
            std::function([](type* self, const type& other) -> decltype(auto) func),    \
            MetaMethods::operator_ ## meta                                              \
        );

#define DEFSINGLE(type, func, meta)                                                     \
        mgr.AddMethod<type>(                                                            \
            std::function([](type* self) -> decltype(auto) func),                       \
            MetaMethods::operator_ ## meta                                              \
        );                                                                              \

        DEFTYPE(ObjectPtr);
        DEFTYPE(SharedObject);

#define DEFINT(num)                                                                     \
        DEFTYPE(num);                                                                   \

        DEFINT(int);
        DEFINT(char);
        DEFINT(size_t);
        DEFINT(int64_t);
        DEFTYPE(float);
        DEFTYPE(bool);

#define DEFVEC(type) DEFVEC_BASE(std::vector<type>, type)
#define DEFVEC_BASE(type, elem)                                         \
        mgr.AddClass<type>();                                           \
        ReflMgrTool::AutoRegister<type>();                              \
        AutoRegister<type::iterator>();                                 \
        DEFCTOR(type);                                                  \
        DEFSINGLE(type, {                                               \
            std::stringstream ss;                                       \
            printVec(ss, *self);                                        \
            return (std::string)(ss.str());                             \
        }, tostring)                                                    \
        mgr.AddMethod(MethodType<void, type, const elem&>::Type(&type::push_back), "push_back");                        \
        mgr.AddMethod(MethodType<elem&, type, size_t>::Type(&type::at), "at");                                          \
        mgr.AddMethod(MethodType<elem&, type, size_t>::Type(&type::operator[]), MetaMethods::operator_index);           \
        mgr.AddMethod(&type::pop_back, "pop_back");                                                                     \

        DEFVEC(ObjectPtr);
        DEFVEC(SharedObject);
        DEFVEC(int);
        DEFVEC(float);
        DEFVEC(std::string);
        DEFVEC_BASE(std::string, char);

        mgr.AddClass<std::string_view>();
        ReflMgrTool::AutoRegister<std::string_view>();
        ReflMgrTool::AutoRegister<std::string_view::iterator>();

#define DEFMAP(key, value)                                                      \
        do {                                                                    \
            using type = std::map<key, value>;                                  \
            mgr.AddClass<type>();                                               \
            AutoRegister<type>();                                               \
            AutoRegister<type::iterator>();                                     \
            mgr.AddMethod(                                                      \
                MethodType<value&, type, const key&>::Type(&type::operator[]),  \
                MetaMethods::operator_index                                     \
            );                                                                  \
            DEFSINGLE(type, {                                                   \
                std::stringstream ss;                                           \
                printMap(ss, *self);                                            \
                return (std::string)(ss.str());                                 \
            }, tostring)                                                        \
        } while (0)

#define DEFMAP_FOR(type)                                \
        DEFMAP(type, int);                              \
        DEFMAP(type, float);                            \
        DEFMAP(type, std::string);                      \
        DEFMAP(type, SharedObject);                     \
        DEFMAP(type, ObjectPtr)

        DEFMAP_FOR(int);
        DEFMAP_FOR(float);
        DEFMAP_FOR(std::string);
        DEFMAP_FOR(SharedObject);
        DEFMAP_FOR(ObjectPtr);

#undef DEFMAP_FOR
#undef DEFMAP
#undef DEFINT
#undef DEFVEC_BASE
#undef DEFVEC
#undef DEFOBJ
#undef DEFSINGLE
#undef DEFDOUBLE
#undef DEFSINGLE_BASE
#undef DEFTYPE
#undef DEFCTOR
    }
    void InitModules() {
        auto& mgr = ReflMgr::Instance();
        mgr.SelfExport();
        // ObjectPtr SharedObject
#define DEFOBJ(Obj)                                                  \
        mgr.AddMethod(&Obj::Invoke, "Invoke");                       \
        mgr.AddMethod(&Obj::GetField, "GetField");                   \
        mgr.AddMethod(&Obj::GetType, "GetType");                     \
        mgr.AddMethod(&Obj::tostring, "tostring");                   \
        mgr.AddMethod(&Obj::ctor, "ctor");                           \
        mgr.AddMethod(&Obj::dtor, "dtor");                           \
        mgr.AddMethod(&Obj::copy, "copy");                           \

        DEFOBJ(ObjectPtr);
        DEFOBJ(SharedObject);
#undef DEFOBJ
    }
    void Init() {
        InitModules();
        InitBaseTypes();
    }
}
