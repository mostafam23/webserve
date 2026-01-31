#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <map>
#include <sys/types.h>
#include "../parsing_validation/ConfigStructs.hpp"

#include <ctime>

struct CgiSession
{
    pid_t pid;
    int pipeOut; // Reading end of the pipe
    int clientFd;
    std::string responseBuffer;
    time_t startTime;
    bool keepAlive;
};

class CgiHandler
{
public:
    static CgiSession startCgi(const std::string &scriptPath, const std::string &method, const std::string &query, const std::string &body, const std::map<std::string, std::string> &headers, int clientFd);
    // executeCgi removed/deprecated to enforce non-blocking rule
};

#endif
