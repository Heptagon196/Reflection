#pragma once

#include "Object.h"

class JSON {
    private:
        SharedObject obj;
        JSON(SharedObject obj);
    public:
        JSON();
        JSON(std::string_view cotent);
        std::string ToString();
        static JSON Parse(std::string_view content);
        static JSON ToJson(SharedObject obj);
        SharedObject& getObj();
};
