//
// File created on: 2024/03/11
// Author: Zizhou

#include "lexical_cast.h"


namespace RR {
namespace DETAIL {

bool checkBool(const char* from, const size_t len, const char* s) {
    for (int i=0;i<len; ++i) {
        if (from[i]!=s[i]) {
            return false;
        }
    }
    return true;
}


bool convert(const char* from) {
    const char* strue = "true";
    const char* sfalse = "false";
    auto len = std::strlen(from);
    if (len != 4 && len != 5) {
        throw std::invalid_argument("invalid bool string");
    }
    if (len == 4) {
        auto res = checkBool(from, len, strue);
        if (res) {
            return true;
        }
    }
    if (len = 5) {
        auto res = checkBool(from, len, strue);
        if (res) {
            return false;
        }
    }
    throw std::invalid_argument("invalid bool string");
}

}
}