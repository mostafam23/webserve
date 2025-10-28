#include "PathUtils.hpp"
#include <sys/stat.h>

bool isDirectory(const std::string &path) {
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return S_ISDIR(s.st_mode);
    return false;
}
