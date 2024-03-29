#include <sstream>
#include "JSON.h"
#include "ReflMgr.h"
#include "MetaMethods.h"

static std::string codeString(std::string_view s) {
    std::string ret = "";
    for (auto ch : s) {
        if (ch == '\n') {
            ret += "\\n";
        } else if (ch == '\r') {
            ret += "\\r";
        } else if (ch == '\t') {
            ret += "\\t";
        } else {
            ret += ch;
        }
    }
    return ret;
}

template<typename T>
static void print(std::stringstream& out, const T& val) {
    if constexpr (std::same_as<T, std::string> || std::same_as<T, std::string_view>) {
        out << '"' << val << '"';
    } else {
        if constexpr (std::same_as<T, SharedObject> || std::same_as<T, ObjectPtr>) {
            if (val.GetType() == TypeID::get<std::string>()) {
                out << '"' << codeString(val.template Get<std::string>()) << '"';
            } else if (val.GetType() == TypeID::get<std::string_view>()) {
                out << '"' << codeString(val.template Get<std::string_view>()) << '"';
            } else {
                out << val;
            }
        } else {
            out << val;
        }
    }
}

static bool useIndent = true;
static int indentWidth = 4;
static int indent = 0;

template<typename T>
static void printVec(std::stringstream& out, T& val) {
    out << "[";
    indent++;
    if (val.size() > 0) {
        print(out, val[0].content());
        for (int i = 1; i < val.size(); i++) {
            out << ", ";
            print(out, val[i].content());
        }
    }
    indent--;
    out << "]";
}

static void printIndent(std::stringstream& out) {
    for (int i = 0; i < indent * indentWidth; i++) {
        out << " ";
    }
}

template<typename T, typename V>
static void printMap(std::stringstream& out, std::unordered_map<T, V>& val) {
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
        print(out, iter->second.content());
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
            print(out, iter->second.content());
            iter++;
        }
    }
    if (useIndent) {
        indent--;
        if (val.size() > 0) {
            out << std::endl;
            printIndent(out);
        }
    }
    out << "}";
}

JSON JSON::NewMap() {
    JSON ret;
    ret.obj = SharedObject::New<std::unordered_map<std::string, JSON>>();
    return ret;
}

JSON JSON::NewVec() {
    JSON ret;
    ret.obj = SharedObject::New<std::vector<JSON>>();
    return ret;
}

void JSON::Init() {
    ReflMgr::Instance().AddMethod<JSON>(std::function([](JSON* self) -> std::string {
        return self->obj.tostring().As<std::string>();
    }), MetaMethods::operator_tostring);
    ReflMgr::Instance().AddMethod<std::vector<JSON>>(std::function([](std::vector<JSON>* self) -> std::string {
        std::stringstream ss;
        printVec(ss, *self);
        return ss.str();
    }), MetaMethods::operator_tostring);
    ReflMgr::Instance().AddMethod<std::unordered_map<std::string, JSON>>(std::function([](std::unordered_map<std::string, JSON>* self) -> std::string {
        std::stringstream ss;
        printMap(ss, *self);
        return ss.str();
    }), MetaMethods::operator_tostring);
}

JSON::JSON(SharedObject obj) : obj(obj) {}

JSON::JSON() {
    obj = SharedObject{ TypeID::get<void>(), nullptr };
}

SharedObject& JSON::content() {
    return obj;
}

std::string JSON::ToString() {
    std::stringstream ss;
    ss << obj;
    return ss.str();
}

JSON JSON::Parse(std::string_view content) {
    return JSON{ content };
}

JSON JSON::ToJson(SharedObject obj) {
    return JSON{ obj };
}

struct Tokenizer {
    std::istream& in;
    Tokenizer(std::istream& in) : in(in) {}
    char ch = 0;
    char getChar() {
        char ret;
        if (ch != 0) {
            ret = ch;
            ch = 0;
            return ret;
        }
        ret = in.get();
        return ret;
    }
    void backChar(char ch) {
        this->ch = ch;
    }
    using Token = std::pair<TypeID, std::string>;
    Token lastToken = { TypeID::get<void>(), "" };
    void backToken(Token token) {
        lastToken = token;
    }
    Token getToken() {
        if (lastToken.second != "") {
            Token ret = lastToken;
            lastToken.second = "";
            return ret;
        }
        char ch = getChar();
        while (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
            ch = getChar();
        }
        if (ch == EOF) {
            return { TypeID::get<void>(), "" };
        }
        std::string content;
        content = ch;
        if (ch == '"') {
            content = "";
            ch = getChar();
            while (ch != '"') {
                if (ch == '\\') {
                    ch = getChar();
                    if (ch == 'n') {
                        ch = '\n';
                    } else if (ch == 'r') {
                        ch = '\r';
                    } else if (ch == 't') {
                        ch = '\t';
                    }
                }
                content.push_back(ch);
                ch = getChar();
            }
            return { TypeID::get<std::string>(), content };
        } else if (isdigit(ch) || isalpha(ch) || ch == '-' || ch == '+' || ch == '.') {
            bool hasAlpha = isalpha(ch);
            bool hasDot = ch == '.';
            ch = getChar();
            while (isdigit(ch) || isalpha(ch) || ch == '-' || ch == '+' || ch == '.') {
                if (isalpha(ch)) {
                    hasAlpha = true;
                }
                if (ch == '.') {
                    if (hasDot) {
                        hasAlpha = true;
                    }
                    hasDot = true;
                }
                content.push_back(ch);
                ch = getChar();
            }
            backChar(ch);
            if (hasAlpha) {
                return { TypeID::get<std::string>(), content };
            } else if (hasDot) {
                return { TypeID::get<float>(), content };
            } else {
                return { TypeID::get<int>(), content };
            }
        } else {
            return { TypeID::get<void>(), content };
        }
    }
    void expectSymbol(Token token, std::string_view s) {
        if (token.first != TypeID::get<void>() || token.second != s) {
            std::cerr << "Error when parsing JSON: expecting " << s <<  ", but " << token.second << " found." << std::endl;
        }
    }
    static Token symbol(char ch) {
        std::string s;
        s = ch;
        return { TypeID::get<void>(), s };
    }
};

template<typename T, typename U>
T ConvertTo(U orig) {
    std::stringstream ss;
    ss << orig;
    T ret;
    ss >> ret;
    return ret;
}

SharedObject parse(Tokenizer& tk) {
    auto token = tk.getToken();
    if (token == Tokenizer::symbol('{')) {
        auto obj = std::unordered_map<std::string, JSON>();
        token = tk.getToken();
        while (token != Tokenizer::symbol('}')) {
            auto key = token.second;
            token = tk.getToken();
            tk.expectSymbol(token, ":");
            obj[(std::string)key] = JSON{parse(tk)};
            token = tk.getToken();
            if (token != Tokenizer::symbol(',')) {
                tk.expectSymbol(token, "}");
                continue;
            }
            token = tk.getToken();
        }
        return SharedObject::New<decltype(obj)>(obj);
    } else if (token == Tokenizer::symbol('[')) {
        auto vec = std::vector<JSON>();
        token = tk.getToken();
        while (token != Tokenizer::symbol(']')) {
            tk.backToken(token);
            vec.push_back(JSON{parse(tk)});
            token = tk.getToken();
            if (token != Tokenizer::symbol(',')) {
                tk.expectSymbol(token, "]");
                continue;
            }
            token = tk.getToken();
        }
        return SharedObject::New<decltype(vec)>(vec);
    } else if (token.first == TypeID::get<int>()) {
        return SharedObject::New<int>(ConvertTo<int>(token.second));
    } else if (token.first == TypeID::get<float>()) {
        return SharedObject::New<float>(ConvertTo<float>(token.second));
    } else if (token.first == TypeID::get<std::string>()) {
        return SharedObject::New<std::string>(token.second);
    } else {
        std::cerr << "Error when parsing JSON: unknown token: type = " << token.first.getName() << ", content = " << token.second << std::endl;
    }
    return SharedObject::Null;
}

JSON::JSON(std::string_view content) {
    std::stringstream ss;
    ss << content;
    Tokenizer tk(ss);
    obj = parse(tk);
}

std::istream& operator >> (std::istream& in, JSON& obj) {
    Tokenizer tk(in);
    obj.obj = parse(tk);
    return in;
}

std::ostream& operator << (std::ostream& out, const JSON& obj) {
    out << obj.obj;
    return out;
}

int JSON::VecSize() {
    return obj.As<std::vector<JSON>>().size();
}

int JSON::MapSize() {
    return obj.As<std::unordered_map<std::string, JSON>>().size();
}

bool JSON::HasKey(const std::string& idx) {
    auto& m = obj.As<std::unordered_map<std::string, JSON>>();
    return m.find(idx) != m.end();
}

bool JSON::HasKey(int idx) {
    return idx < obj.As<std::vector<JSON>>().size() && idx > 0;
}

JSON& JSON::operator[] (std::string_view idx) {
    return obj.As<std::unordered_map<std::string, JSON>>()[(std::string)idx];
}

JSON& JSON::operator[] (int idx) {
    return obj.As<std::vector<JSON>>()[idx];
}

JSON& JSON::operator = (int value) {
    obj = SharedObject::New<int>(value);
    return *this;
}

JSON& JSON::operator = (std::string_view value) {
    obj = SharedObject::New<std::string>((std::string)value);
    return *this;
}

JSON& JSON::operator = (const JSON& other) {
    obj = other.obj;
    return *this;
}

void JSON::AddItem(JSON item) {
    obj.As<std::vector<JSON>>().push_back(item);
}

void JSON::AddItem(std::string key, JSON item) {
    obj.As<std::unordered_map<std::string, JSON>>()[key] = item;
}

void JSON::Foreach(std::function<void(std::string_view key, JSON& item)> call) {
    auto& m = obj.As<std::unordered_map<std::string, JSON>>();
    for (auto& p : m) {
        call(p.first, p.second);
    }
}

void JSON::Foreach(std::function<void(int idx, JSON& item)> call) {
    auto& m = obj.As<std::vector<JSON>>();
    for (int idx = 0; idx < m.size(); idx++) {
        call(idx, m[idx]);
    }
}

void Foreach(std::function<void(int idx, JSON& item)> call);

void JSON::RemoveItem(int pos) {
    auto& vec = obj.As<std::vector<JSON>>();
    vec.erase(vec.begin() + pos);
}

void JSON::RemoveItem(std::string key) {
    obj.As<std::unordered_map<std::string, JSON>>().erase(key);
}
