#include "Logger.hpp"
#include <iostream>
#include <sstream>

namespace {
    const char* C_RESET = "\033[0m";
    const char* C_INFO = "\033[36m";     // cyan
    const char* C_REQ  = "\033[35m";     // magenta
    const char* C_DEBUG = "\033[33m";    // yellow
}

namespace Logger {
    bool isLoggingEnabled() 
    { 
        return true; 
    }

    static void logColored(const char* color, const std::string& tag, const std::string& msg) 
    {
        if (!isLoggingEnabled()) return;
        std::cout << color << tag << " " << msg << C_RESET << std::endl;
    }

    void info(const std::string &msg)
    { 
        logColored(C_INFO, "[INFO]", msg); 
    }
    void request(const std::string &msg) 
    { 
        logColored(C_REQ,  "[REQUEST]", msg); 
    }
    void debug(const std::string &msg)
    {
        logColored(C_DEBUG, "[DEBUG]", msg);
    }
}

