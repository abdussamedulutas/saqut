#ifndef SAQUT_TOOLS
#define SAQUT_TOOLS

#include <string>

inline std::string padRight(std::string str, size_t totalLen) {
    if (str.size() < totalLen) {
        str.append(totalLen - str.size(), ' ');
    }
    return str;
}

#endif
