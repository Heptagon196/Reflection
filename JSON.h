#pragma once

#include "Object.h"

class JSON {
    private:
        SharedObject obj;
    public:
        static void Init();
        SharedObject& content();
        JSON();
        explicit JSON(SharedObject obj);
        JSON(std::string_view cotent);
        std::string ToString();
        void AddItem(JSON item);
        void AddItem(std::string key, JSON item);
        void RemoveItem(int pos);
        void RemoveItem(std::string key);
        static JSON Parse(std::string_view content);
        static JSON ToJson(SharedObject obj);
        static JSON NewMap();
        static JSON NewVec();
        friend std::istream& operator >> (std::istream& in, JSON& obj);
        friend std::ostream& operator << (std::ostream& out, const JSON& obj);
        JSON& operator[] (std::string_view idx);
        JSON& operator[] (int idx);
        JSON& operator = (int value);
        JSON& operator = (std::string_view value);
        JSON& operator = (const JSON& other);
};
