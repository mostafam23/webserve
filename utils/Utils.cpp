#include "Utils.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>
#include <iomanip>
#include <cstdlib>

bool isDirectory(const std::string &path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return S_ISDIR(s.st_mode);
    return false;
}

std::string generateDirectoryListing(const std::string &path, const std::string &requestPath)
{
    DIR *dir;
    struct dirent *ent;
    std::ostringstream oss;

    oss << "<html><head><title>Index of " << requestPath << "</title></head><body>";
    oss << "<h1>Index of " << requestPath << "</h1><hr><pre>";

    if ((dir = opendir(path.c_str())) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            std::string name = ent->d_name;
            if (name == ".")
                continue;

            std::string href = name;
            if (isDirectory(path + "/" + name))
            {
                href += "/";
                name += "/";
            }

            oss << "<a href=\"" << href << "\">" << name << "</a><br>";
        }
        closedir(dir);
    }
    else
    {
        oss << "Error reading directory";
    }

    oss << "</pre><hr></body></html>";
    return oss.str();
}

long parseSize(const std::string &sizeStr)
{
    if (sizeStr.empty())
        return 0;
    long size = 0;
    size_t i = 0;
    while (i < sizeStr.size() && isdigit(sizeStr[i]))
    {
        size = size * 10 + (sizeStr[i] - '0');
        i++;
    }
    if (i < sizeStr.size())
    {
        char unit = tolower(sizeStr[i]);
        if (unit == 'k')
            size *= 1024;
        else if (unit == 'm')
            size *= 1024 * 1024;
        else if (unit == 'g')
            size *= 1024 * 1024 * 1024;
    }
    return size;
}
