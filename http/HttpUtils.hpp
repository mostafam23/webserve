#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include <cstddef>
#include <map>
#include <string>
#include <cstring>

std::string intToString(int n);
size_t getRequestLength(const char *buffer, size_t length);
std::string buildErrorResponse(int code, const std::string &message);
std::map<std::string, std::string> parseHeaders(const std::string &request);
std::string unchunkBody(const std::string &body);

#endif // HTTP_UTILS_HPP
