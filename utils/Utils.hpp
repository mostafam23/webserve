#ifndef PATH_UTILS_HPP
#define PATH_UTILS_HPP

#include <string>
#include <climits>
#include <vector>

bool isDirectory(const std::string &path);
std::string generateDirectoryListing(const std::string &path, const std::string &requestPath);
long parseSize(const std::string &sizeStr);

#endif // PATH_UTILS_HPP
