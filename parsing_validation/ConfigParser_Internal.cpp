#include "ConfigParser_Internal.hpp"
#include <string>

std::string cp_removeComments(const std::string &line)
{
    size_t pos = line.find('#');
    if (pos != std::string::npos)
        return line.substr(0, pos);
    return line;
}
