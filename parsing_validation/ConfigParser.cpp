// Intentionally left minimal — implementations moved to ConfigParser_Parse.cpp
#include "ConfigParser.hpp"
#include "../utils/Utils.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void throwError(const std::string& msg, int lineNum) {
    std::ostringstream oss;
    oss << "Error: " << msg << " at line " << lineNum;
    throw std::runtime_error(oss.str());
}

ConfigParser::ConfigParser(const std::string &file)
{
    this->filename = file;
}

bool ConfigParser::validateConfigFile(const std::string &filename)
{
    ConfigValidator validator(filename);
    return validator.validate();
}

// helpers are implemented in ConfigParser_Utils.cpp and cp_removeComments() in ConfigParser_Internal.cpp

Servers ConfigParser::parseServers()
{
    Servers servers;

    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
        throwError("Cannot open config file", 0);
    }

    std::string line;
    Location currentLoc;
    bool inLocation = false;
    bool inServer = false;
    int lineNum = 0;
    Server currentServer;

    while (ft_getline(file, line))
    {
        lineNum++;

        line = removeComments(line);
        line = trim(line);

        if (line.empty())
            continue;

        // Detect start of a server block
        if (line.find("server") == 0 && !inServer)
        {
            std::string afterServer = ft_substr(line, 6);
            afterServer = trim(afterServer);

            if (afterServer.empty())
            {
                // "server" alone - look for { on next line
                inServer = true;
                currentServer = Server(); // Reset for new server
                continue;
            }
            else if (afterServer == "{" || afterServer.find("{") == 0)
            {
                // "server {" on same line
                inServer = true;
                currentServer = Server(); // Reset for new server
                continue;
            }
            else
            {
                throwError("Unexpected text after 'server'", lineNum);
            }
        }

        // If we're waiting for opening brace after server
        if (inServer && line == "{")
            continue;

        if (!inServer)
            continue;

        // Check for semicolons
        if (lineRequiresSemicolon(line) && line[line.size() - 1] != ';')
            throwError("Missing semicolon", lineNum);

        if (line.find("}") == 0)
        {
            if (inLocation)
            {
                if (currentServer.location_count < 10)
                    currentServer.locations[currentServer.location_count++] = currentLoc;
                else
                    std::cerr << "Warning: Maximum 10 locations supported. Ignoring location: "
                              << currentLoc.path << std::endl;
                inLocation = false;
            }
            else if (inServer)
            {
                inServer = false;

                if (currentServer.host.empty())
                    throwError("Missing required directive 'host'", lineNum);
                if (currentServer.max_size.empty())
                    throwError("Missing required directive 'max_size'", lineNum);
                if (currentServer.server_name.empty())
                    throwError("Missing required directive 'server_name'", lineNum);
                if (currentServer.root.empty())
                    throwError("Missing required directive 'root'", lineNum);
                if (currentServer.index.empty())
                    throwError("Missing required directive 'index'", lineNum);
                if (currentServer.location_count == 0)
                    throwError("Server must have at least one location", lineNum);

                // Add completed server to servers collection
                servers.addServer(currentServer);
                continue;
            }
            continue;
        }

        // Parse listen directive
        if (line.find("listen") == 0)
        {
            std::string val = getValue(line);
            if (!val.empty())
            {
                size_t colonPos = val.find(':');
                if (colonPos != std::string::npos)
                {
                    currentServer.host = ft_substr(val, 0, colonPos);
                    std::string portStr = ft_substr(val, colonPos + 1);
                    currentServer.listen = ft_atoi(portStr.c_str());
                }
                else
                {
                    currentServer.listen = ft_atoi(val.c_str());
                }
            }
            else
            {
                throwError("Missing value for 'listen'", lineNum);
            }
        }
        else if(line.find("host") == 0 && currentServer.host.empty())
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'host'", lineNum);
            }
            currentServer.host = val;
        }
        else if (line.find("max_size") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'max_size'", lineNum);
            }
            currentServer.max_size = val;
        }
        else if (line.find("server_name") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'server_name'", lineNum);
            }
            currentServer.server_name = val;
        }
        else if (line.find("root") == 0 && !inLocation)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'root'", lineNum);
            }
            currentServer.root = val;
        }
        else if (line.find("index") == 0 && !inLocation)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'index'", lineNum);
            }
            currentServer.index = val;
        }
        else if (line.find("error_page") == 0 && !inLocation)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'error_page'", lineNum);
            }
            size_t spacePos = val.find(' ');
            if (spacePos != std::string::npos)
            {
                int errorCode = ft_atoi(ft_substr(val, 0, spacePos).c_str());
                std::string errorPath = ft_substr(val, spacePos + 1);
                currentServer.error_pages[errorCode] = errorPath;
            }
            else
            {
                throwError("Invalid error_page format", lineNum);
            }
        }
        else if (line.find("location") == 0 && !inLocation)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'location'", lineNum);
            }
            currentLoc = Location();
            currentLoc.path = val;
            inLocation = true;
            continue;
        }

        // Parse location-specific directives
        if (inLocation)
        {
            if (line.find("methods") == 0)
            {
                std::string val = getValue(line);
                if (val.empty())
                {
                    throwError("Missing value for 'methods'", lineNum);
                }
                std::istringstream iss(val);
                std::string method;
                while (iss >> method)
                {
                    if (!method.empty() && method[method.size() - 1] == ';')
                        method = ft_substr(method, 0, method.size() - 1);
                    
                    currentLoc.methods.insert(method);

                    if (method == "GET")
                        currentLoc.allow_get = true;
                    else if (method == "POST")
                        currentLoc.allow_post = true;
                    else if (method == "DELETE")
                        currentLoc.allow_delete = true;
                }
            }
            else if (line.find("root") == 0)
            {
                std::string val = getValue(line);
                if (val.empty())
                {
                    throwError("Missing value for 'root'", lineNum);
                }
                currentLoc.root = val;
            }
            else if (line.find("cgi_extension") == 0)
            {
                std::string val = getValue(line);
                if (val.empty())
                {
                    throwError("Missing value for 'cgi_extension'", lineNum);
                }
                // Split by space to allow multiple extensions
                std::istringstream iss(val);
                std::string ext;
                while (iss >> ext) {
                    if (!ext.empty() && ext[ext.size() - 1] == ';')
                        ext = ft_substr(ext, 0, ext.size() - 1);
                    currentLoc.cgi_extensions.push_back(ext);
                }
            }
            else if (line.find("upload_path") == 0)
            {
                std::string val = getValue(line);
                if (val.empty())
                {
                    throwError("Missing value for 'upload_path'", lineNum);
                }
                currentLoc.upload_path = val;
            }
            else if (line.find("autoindex") == 0)
            {
                std::string val = getValue(line);
                if (val == "on")
                    currentLoc.autoindex = true;
                else if (val == "off")
                    currentLoc.autoindex = false;
            }
            else if (line.find("index") == 0)
            {
                std::string val = getValue(line);
                if (val.empty())
                {
                    throwError("Missing value for 'index'", lineNum);
                }
                currentLoc.index = val;
            }
            else if (line.find("return") == 0)
            {
                std::istringstream iss(line);
                std::string key;
                int code;
                std::string url;
                iss >> key >> code >> url;
                if (!url.empty() && url[url.size() - 1] == ';')
                    url = ft_substr(url, 0, url.size() - 1);
                currentLoc.redirect = std::make_pair(code, url);
            }
        }
    }

    return servers;
}
