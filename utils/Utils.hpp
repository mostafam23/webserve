#ifndef PATH_UTILS_HPP
#define PATH_UTILS_HPP

#include <string>
#include <climits>
#include <vector>

bool isDirectory(const std::string &path);
int ft_atoi(const char *str);
std::string generateDirectoryListing(const std::string &path, const std::string &requestPath);
long parseSize(const std::string &sizeStr);

#endif // PATH_UTILS_HPP
