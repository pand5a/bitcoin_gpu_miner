//
// Created by fly on 2020/6/16.
//

#ifndef BITCOIN_GPU_MINER_EASE_LOG_H
#define BITCOIN_GPU_MINER_EASE_LOG_H

#include <string>
#include <iostream>
#include <stdarg.h>

#include "common.h"

//the following are UBUNTU/LINUX ONLY terminal color codes.
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

template<typename T>
struct item_return {
    using type = T&&;
};

template<typename T>
inline typename item_return<T>::type convert(T&& arg) {
    return static_cast<T&&>(arg);
}

class AppLog {
    template<typename... Args>
    // static void FmtOut(const char* color, const char* level, const char* format, Args&&... args) {
    static void FmtOut(const char* color, const char* level, const std::string& format, Args&&... args) {
        char buf[4096] = {0};
        // auto argv =  sizeof...(args);
        snprintf(buf, sizeof(buf), format.c_str(), convert(std::forward<Args>(args))...);
        std::cout << color << CurrentTime() << " " << level << ": " << buf << std::endl;
    }

public:
    template<typename...Args>
    static std::string Fmt(const char* format, Args&&... args) {
        char buf[4096] = {0};
        snprintf(buf, sizeof(buf), format, convert(std::forward<Args>(args))...);
        return std::move(std::string(buf));
    }

    static void CheckError(int code, const std::string& error = "");

    template<typename T>
    static void CheckError(T* t, const std::string& error = "") {
        if (t == nullptr) {
            std::cout << error << std::endl;
            exit(1);
        }
    }

    template<typename... Args>
    static void Info(const char* format,Args&&... args) {
        FmtOut(BLUE, "INFO ", format, convert(std::forward<Args>(args))...);
    }

    template<typename... Args>
    static void Warn(const char* format,Args&&... args) {
        FmtOut(MAGENTA, "WARN", format, convert(std::forward<Args>(args))...);
    }

    template<typename... Args>
    static void Error(const char* format,Args&&... args) {
        FmtOut(RED, "ERROR", format, convert(std::forward<Args>(args))...);
    }

    template<typename... Args>
    static void Debug(const char* format,Args&&... args) {
        FmtOut(YELLOW, "DEBUG", format, convert(std::forward<Args>(args))...);
    }

};


#endif //BITCOIN_GPU_MINER_EASE_LOG_H
