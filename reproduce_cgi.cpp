#include "server/CgiHandler.hpp"
#include "parsing_validation/ConfigStructs.hpp"
#include <iostream>
#include <map>
#include <vector>

// Mock Server and Location
int main() {
    Server server;
    Location loc;
    loc.cgi_extension = ".py";
    loc.root = "./www";

    std::string scriptPath = "./www/cgi-bin/env_info.py";
    std::string method = "GET";
    std::string query = "";
    std::string body = "";
    std::map<std::string, std::string> headers;

    std::cout << "Executing CGI: " << scriptPath << std::endl;
    std::string output = CgiHandler::executeCgi(scriptPath, method, query, body, headers, server, loc);
    
    std::cout << "Output size: " << output.size() << std::endl;
    std::cout << "Output content:\n" << output << std::endl;

    return 0;
}
