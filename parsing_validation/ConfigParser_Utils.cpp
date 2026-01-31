#include "ConfigParser.hpp"
#include "../utils/Utils.hpp"

std::string ConfigParser::getValue(const std::string &line)
{
    size_t pos = line.find_first_of(" \t");
    if (pos == std::string::npos)
        return "";

    size_t start = line.find_first_not_of(" \t", pos);
    if (start == std::string::npos)
        return "";

    std::string val = ft_substr(line, start);
    val = trim(val);

    if (!val.empty() && val[val.length() - 1] == ';')
        val = ft_substr(val, 0, val.length() - 1);

    if (!val.empty() && val[val.length() - 1] == '{')
        val = ft_substr(val, 0, val.length() - 1);

    return trim(val);
}

bool ConfigParser::lineRequiresSemicolon(const std::string &line)
{
    if (line.empty() || line[0] == '#' ||
        line.find("{") != std::string::npos ||
        line.find("}") != std::string::npos ||
        line.find("location") == 0 ||
        line.find("server") == 0)
        return false;
    return true;
}