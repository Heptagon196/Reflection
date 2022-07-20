#include <iostream>
#include <functional>
#include <sstream>
#include "ReflMgrInit.h"
#include "ReflMgr.h"
#include "JSON.h"

struct Adder {
    int p = 123;
    void add(int a, int b) {
        std::cout << a << " + " << b << " = " << a + b << std::endl;
    }
};

struct P : public Adder {
    int x;
    std::string s;
    P() {}
    P(int x, std::string s) : x(x), s(s) {}
    void print() {
        std::cout << "Hello World!" << std::endl;
    }
    void print(int i) {
        std::cout << "Hello World!2" << std::endl;
    }
};

void selfTest() {
    ReflMgrTool::Init();
    auto mgr = ReflMgr::Instance().InvokeStatic(TypeID::get<ReflMgr>(), "Instance", {});
    mgr.Invoke("AddMethod", {
        SharedObject::New<TypeID>(TypeID::get<P>()),
        SharedObject::New<std::string_view>("print"),
        SharedObject::New<TypeID>(TypeID::get<void>()),
        SharedObject::New<ArgsTypeList>(),
        SharedObject::New(
            std::function([](ObjectPtr instance, const std::vector<ObjectPtr>& params) -> SharedObject {
                instance.As<P>().print();
                return SharedObject::Null;
            })
        )
    });
    auto p = SharedObject::New<P>();
    mgr.Invoke("Invoke", {
        SharedObject::New<ObjectPtr>(TypeID::get<P>(), p.GetRawPtr()),
        SharedObject::New<std::string_view>("print"),
        SharedObject::New<std::vector<ObjectPtr>>(),
    });
    SharedObject::New<ObjectPtr>(p.ToObjectPtr()).Invoke("Invoke", { SharedObject::New<std::string_view>("print"), SharedObject::New<std::vector<ObjectPtr>>() });
}

void overloadTest() {
    auto& mgr = ReflMgr::Instance();
    mgr.AddMethod(MethodType<void, P, int>::Type(&P::print), "print");
    mgr.AddMethod(MethodType<void, P>::Type(&P::print), "print");
    auto p = SharedObject::New<P>();
    p.Invoke("print", { SharedObject::New<int>(123) });
    p.Invoke("print", {});
    p.Invoke("print", { SharedObject::New<int>(123), SharedObject::New<int>(123) });
}

void metaTest() {
    SharedObject s = ReflMgr::Instance().New<std::vector<int>>();
    for (int i = 0; i < 5; i++) {
        s.Invoke("push_back", { SharedObject::New<int>(i) });
    }
    SharedObject p = ReflMgr::Instance().New<int>();
    for (SharedObject i : s) {
        std::cout << i << std::endl;
        i++;
    }
    auto var = SharedObject::New<int>(2);
    s[var]++;
    std::cout << s << std::endl;
}

int main() {
    ReflMgrTool::Init();
    JSON json("[1, 2, 3, 4]");
    std::cout << json.ToString() << std::endl;
    // metaTest();
}
