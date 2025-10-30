#include "Logger.hpp"
#include <iostream>
#include <sstream>

namespace {
const char* C_RESET = "\033[0m";
const char* C_INFO = "\033[36m";     // cyan
const char* C_REQ  = "\033[35m";     // magenta
const char* C_WARN = "\033[33m";     // yellow
const char* C_ERR  = "\033[31m";     // red
}

namespace Logger {
bool isLoggingEnabled() { return true; }

static void logColored(const char* color, const std::string& tag, const std::string& msg) {
    if (!isLoggingEnabled()) return;
    std::cout << color << tag << " " << msg << C_RESET << std::endl;
}

void info(const std::string &msg)    { logColored(C_INFO, "[INFO]", msg); }
void request(const std::string &msg) { logColored(C_REQ,  "[REQUEST]", msg); }
void warn(const std::string &msg)    { logColored(C_WARN, "[WARN]", msg); }
void error(const std::string &msg)   { logColored(C_ERR,  "[ERROR]", msg); }
}

