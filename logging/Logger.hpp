#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>

namespace Logger {
bool isLoggingEnabled();
void info(const std::string &msg);
void request(const std::string &msg);
void warn(const std::string &msg);
void error(const std::string &msg);
}

#endif
