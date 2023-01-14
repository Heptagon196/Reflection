#include "ReflMgr.h"

ReflMgr& ReflMgr::Instance() {
    static ReflMgr instance;
    return instance;
}
