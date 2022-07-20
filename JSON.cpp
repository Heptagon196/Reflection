#include <sstream>
#include "JSON.h"
#include "ReflMgr.h"

JSON::JSON(SharedObject obj) : obj(obj) {}

JSON::JSON() {
    obj = SharedObject{ TypeID::get<void>(), nullptr };
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

SharedObject& JSON::getObj() {
    return obj;
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
        auto obj = std::map<std::string, SharedObject>();
        token = tk.getToken();
        while (token != Tokenizer::symbol('}')) {
            auto key = token.second;
            token = tk.getToken();
            tk.expectSymbol(token, ":");
            obj[(std::string)key] = parse(tk);
            token = tk.getToken();
            if (token != Tokenizer::symbol(',')) {
                tk.expectSymbol(token, "}");
                continue;
            }
            token = tk.getToken();
        }
        return SharedObject::New<decltype(obj)>(obj);
    } else if (token == Tokenizer::symbol('[')) {
        auto vec = std::vector<SharedObject>();
        token = tk.getToken();
        while (token != Tokenizer::symbol(']')) {
            tk.backToken(token);
            vec.push_back(parse(tk));
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
