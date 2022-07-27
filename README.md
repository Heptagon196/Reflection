简单的 C++ 反射库，思路参考 https://github.com/Ubpa/UDRefl

并使用反射实现了简单的 JSON 库

示例


```C++
struct Test {
    int p = 123;
    void add(int a, int b) {
        std::cout << a << " + " << b << " = " << a + b << std::endl;
    }
};

void test1() {
    ReflMgrTool::Init(); // 自动注册常用类型

    auto& mgr = ReflMgr::Instance();
    mgr.AddClass<Test>();
    mgr.AddField(&Test::p, "p");
    mgr.AddMethod(&Test::add, "add");

    auto instance = mgr.New(TypeID::get<Test>());
    std::cout << instance.GetField("p") << std::endl;
    instance.Invoke("add", { SharedObject::New<int>(1), SharedObject::New<int>(2) });
}
```

支持自动注册部分方法


```C++
void test2() {
    ReflMgrTool::AutoRegister<int>(); // 自动注册元方法
    ReflMgrTool::AutoRegister<std::vector<int>>();
    ReflMgrTool::AutoRegister<std::vector<int>::iterator>();

    auto& mgr = ReflMgr::Instance();
    mgr.AddMethod( // 手动注册元方法
        std::function([](std::vector<int>* self) {
            std::cout << '[' << (*self)[0];
            for (int i = 1; i < self->size(); i++) {
                std::cout << ", " << (*self)[i];
            }
            std::cout << ']';
        }),
        MetaMethods::operator_tostring
    );

    SharedObject s = mgr.New<std::vector<int>>();
    for (int i = 0; i < 5; i++) {
        s.Invoke("push_back", { SharedObject::New<int>(i) });
    }
    SharedObject p = mgr.New<int>();
    for (SharedObject i : s) {
        std::cout << i << std::endl;
    }
    std::cout << s << std::endl;
}
```

继承
```C++
struct Adder {
    int p = 123;
    void add(int a, int b) {
        std::cout << p << ": " << a << " + " << b << " = " << a + b << std::endl;
    }
};

struct Test {
    std::string val = "_test";
    void test() {
        std::cout << "test" << val << std::endl;
    }
};

struct P : public Adder, public Test {};

void inheritanceTest() {
    ReflMgrTool::Init();
    auto& mgr = ReflMgr::Instance();

    mgr.AddClass<P>();
    mgr.SetInheritance<P, Adder, Test>(); // 声明继承关系

    mgr.AddMethod(&Adder::add, "add");
    mgr.AddMethod(&Test::test, "test");
    mgr.AddField(&Adder::p, "p");
    mgr.AddField(&Test::val, "val");

    auto instance = mgr.New<P>();
    std::cout << instance.GetField("p") << std::endl;
    std::cout << instance.GetField("val") << std::endl;
    instance.Invoke("test");
    instance.Invoke("add", { SharedObject::New<int>(1), SharedObject::New<int>(2) });
}
```


JSON 库
```C++
ReflMgrTool::Init();
JSON::Init();
JSON data("{a:{arr:[1,2,\"hello\"]}, b:1, c:temp}");
data["a"]["arr"][0] = "abc";
data.RemoveItem("b");
data.AddItem("d", JSON::NewMap());
data["d"].AddItem("1", JSON{"temp"});
data["a"]["arr"].AddItem(JSON{"test"});
std::cout << data << std::endl;
std::cout << data["a"] << std::endl;
std::cout << data["a"]["arr"] << std::endl;
std::cout << data["a"]["arr"][1] << " " << data["a"]["arr"][2] << std::endl;
std::cin >> data;
std::cout << data << std::endl;
```
