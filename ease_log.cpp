//
// Created by fly on 2020/6/16.
//

#include "ease_log.h"




//template<typename... Args>
//std::string fmt(const char* format, Args&&... args) {
//    char buf[4096] = {0};
//    snprintf(buf, sizeof(buf), format, convert(std::forward<Args>(args))...);
//    return buf;
//}

void AppLog::CheckError(int code, const std::string &error) {
    if (code != 0) {
        std::cout << "code: " << code << ", "<< error << std::endl;
        exit(code);
    }
}


