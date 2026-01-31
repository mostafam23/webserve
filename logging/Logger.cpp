#include "Logger.hpp"
#include <iostream>
#include <sstream>

namespace {
    const char* C_RESET = "\033[0m";
    const char* C_INFO = "\033[90m";     // bright black (gray) - for connections
    const char* C_REQ  = "\033[35m";     // magenta
    const char* C_UPLOAD = "\033[32m";   // green
    const char* C_GET    = "\033[34m";   // blue
    const char* C_POST   = "\033[33m";   // yellow
    const char* C_DELETE = "\033[31m";   // red
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
        const char* color = C_REQ;
        if (msg.find("GET") != std::string::npos) color = C_GET;
        else if (msg.find("POST") != std::string::npos) color = C_POST;
        else if (msg.find("DELETE") != std::string::npos) color = C_DELETE;

        logColored(color,  "[REQUEST]", msg); 
    }
    void upload(const std::string &msg)
    {
        logColored(C_UPLOAD, "[UPLOAD]", msg);
    }
}

