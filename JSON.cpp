#include <sstream>
#include "JSON.h"
#include "ReflMgr.h"
#include "MetaMethods.h"

template<typename T>
static void print(std::stringstream& out, const T& val) {
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
static void printVec(std::stringstream& out, T& val) {
    out << "[";
    if (val.size() > 0) {
        out << " ";
        print(out, val[0].content());
        for (int i = 1; i < val.size(); i++) {
            out << ", ";
            print(out, val[i].content());
        }
    }
    out << " ]";
}

static bool useIndent = true;
static int indentWidth = 2;
static int indent = 0;

static void printIndent(std::stringstream& out) {
    for (int i = 0; i < indent * indentWidth; i++) {
        out << " ";
    }
}

template<typename T, typename V>
static void printMap(std::stringstream& out, std::map<T, V>& val) {
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

void JSON::Init() {
    ReflMgr::Instance().AddMethod<JSON>(std::function([](JSON* self) -> std::string {
        return self->obj.tostring().As<std::string>();
    }), MetaMethods::operator_tostring);
    ReflMgr::Instance().AddMethod<std::vector<JSON>>(std::function([](std::vector<JSON>* self) -> std::string {
        std::stringstream ss;
        printVec(ss, *self);
        return ss.str();
    }), MetaMethods::operator_tostring);
    ReflMgr::Instance().AddMethod<std::map<std::string, JSON>>(std::function([](std::map<std::string, JSON>* self) -> std::string {
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
        auto obj = std::map<std::string, JSON>();
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

JSON& JSON::operator[] (std::string_view idx) {
    return obj.As<std::map<std::string, JSON>>()[(std::string)idx];
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
