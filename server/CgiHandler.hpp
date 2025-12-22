#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <map>
#include "../parsing_validation/ConfigStructs.hpp"

class CgiHandler
{
public:
    static std::string executeCgi(const std::string &scriptPath, const std::string &method, const std::string &query, const std::string &body, const std::map<std::string, std::string> &headers, const Server &server, const Location &loc);
};

#endif
