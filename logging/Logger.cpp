#include "Logger.hpp"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#if defined(_WIN32)
#  include <windows.h>
#else
#  include <sys/time.h>
#endif

namespace {
bool g_debug_enabled = false;

inline bool envDebug() {
    const char* v = std::getenv("DEBUG");
    return v && (v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y');
}

const char* C_RESET = "\033[0m";
const char* C_INFO = "\033[36m";     // cyan
const char* C_REQ  = "\033[35m";     // magenta
const char* C_WARN = "\033[33m";     // yellow
const char* C_ERR  = "\033[31m";     // red

static std::string nowTimestamp()
{
    std::time_t t = std::time(NULL);
    std::tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
    SYSTEMTIME st;
    GetLocalTime(&st);
    int ms = (int)st.wMilliseconds;
#else
    localtime_r(&t, &tm_buf);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int ms = (int)(tv.tv_usec / 1000);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    std::ostringstream oss;
    oss << buf << "." << std::setw(3) << std::setfill('0') << ms;
    return oss.str();
}
}

namespace Logger {
void setDebugEnabled(bool enabled) { g_debug_enabled = enabled; }

bool isDebugEnabled() {
    return g_debug_enabled || envDebug();
}

static void logColored(const char* color, const std::string& tag, const std::string& msg) {
    if (!isDebugEnabled()) return;
    std::cout << color << tag << " [" << nowTimestamp() << "] " << msg << C_RESET << std::endl;
}

void info(const std::string &msg)    { logColored(C_INFO, "[INFO]", msg); }
void request(const std::string &msg) { logColored(C_REQ,  "[REQUEST]", msg); }
void warn(const std::string &msg)    { logColored(C_WARN, "[WARN]", msg); }
void error(const std::string &msg)   { logColored(C_ERR,  "[ERROR]", msg); }
}

