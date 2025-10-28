// Intentionally left minimal — implementations moved to ConfigParser_Parse.cpp
#include "ConfigParser.hpp"
#include "ConfigParser_Internal.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>

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

Server ConfigParser::parseServer()
{
    Server server;

    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    Location currentLoc;
    bool inLocation = false;
    bool inServer = false;
    int lineNum = 0;

    while (std::getline(file, line))
    {
        lineNum++;

        line = cp_removeComments(line);
        line = trim(line);

        if (line.empty())
            continue;

        // Detect start of a server block
        if (line.find("server") == 0 && !inServer)
        {
            std::string afterServer = line.substr(6);
            afterServer = trim(afterServer);

            if (afterServer.empty())
            {
                // "server" alone - look for { on next line
                inServer = true;
                continue;
            }
            else if (afterServer == "{" || afterServer.find("{") == 0)
            {
                // "server {" on same line
                inServer = true;
                continue;
            }
            else
            {
                std::cerr << "Error: Unexpected text after 'server' at line " << lineNum
                          << ": \"" << afterServer << "\"" << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        // If we're waiting for opening brace after server
        if (inServer && line == "{")
        {
            continue;
        }

        if (!inServer)
            continue;

        // Check for semicolons
        if (lineRequiresSemicolon(line) && line[line.size() - 1] != ';')
        {
            std::cerr << "Error: Missing semicolon at line " << lineNum
                      << ": \"" << line << "\"" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (line == "}" || line.find("}") == 0)
        {
            if (inLocation)
            {
                if (server.location_count < 10)
                    server.locations[server.location_count++] = currentLoc;
                else
                    std::cerr << "Warning: Maximum 10 locations supported. Ignoring location: "
                              << currentLoc.path << std::endl;
                inLocation = false;
            }
            else if (inServer)
            {
                inServer = false;
                break;
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
                    server.host = val.substr(0, colonPos);
                    std::string portStr = val.substr(colonPos + 1);
                    server.listen = atoi(portStr.c_str());
                }
                else
                {
                    server.listen = atoi(val.c_str());
                }

                if (server.listen <= 0 || server.listen > 65535)
                {
                    std::cerr << "Error: Invalid port number at line " << lineNum << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                std::cerr << "Error: Missing value for 'listen' at line " << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (line.find("max_size") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                std::cerr << "Error: Missing value for 'max_size' at line " << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }
            server.max_size = val;
        }
        else if (line.find("server_name") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                std::cerr << "Error: Missing value for 'server_name' at line " << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }
            server.server_name = val;
        }
        else if (line.find("root") == 0 && !inLocation)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                std::cerr << "Error: Missing value for 'root' at line " << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }
            server.root = val;
        }
        else if (line.find("index") == 0 && !inLocation)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                std::cerr << "Error: Missing value for 'index' at line " << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }
            server.index = val;
        }
        else if (line.find("error_page") == 0)
        {
            std::istringstream iss(line);
            std::string key;
            int code;
            std::string value;
            iss >> key >> code >> value;

            if (value.empty())
            {
                std::cerr << "Error: Missing value for 'error_page' at line " << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }

            if (!value.empty() && value[value.size() - 1] == ';')
                value = value.substr(0, value.size() - 1);

            server.error_pages[code] = value;
        }
        else if (line.find("location") == 0)
        {
            inLocation = true;
            currentLoc = Location();

            // Extract path - getValue handles both "location path {" and "location path"
            std::string path = getValue(line);
            if (path.empty())
            {
                std::cerr << "Error: Missing path for 'location' at line " << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }
            currentLoc.path = path;

            // Check if brace is on the same line
            if (line.find("{") != std::string::npos)
            {
                // Brace is on same line, we're good
                continue;
            }

            // No brace on same line - look for it on next non-empty lines
            bool foundBrace = false;
            while (std::getline(file, line))
            {
                lineNum++;
                line = trim(cp_removeComments(line));
                
                if (line.empty())
                    continue;
                
                if (line == "{" || line.find("{") == 0)
                {
                    foundBrace = true;
                    break;
                }
                else
                {
                    std::cerr << "Error: Expected '{' after 'location' at line " << lineNum << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            
            if (!foundBrace)
            {
                std::cerr << "Error: Expected '{' after 'location' but reached end of file" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (inLocation && line.find("methods") == 0)
        {
            std::string methods_line = getValue(line);
            std::istringstream iss(methods_line);
            std::string method;
            while (iss >> method)
            {
                if (!method.empty() && method[method.size() - 1] == ';')
                    method = method.substr(0, method.size() - 1);
                if (!method.empty())
                    currentLoc.methods.insert(method);
            }
        }
        else if (inLocation && line.find("root") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                std::cerr << "Error: Missing value for 'root' in location block at line "
                          << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }
            currentLoc.root = val;
        }
        else if (inLocation && line.find("cgi_extension") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                std::cerr << "Error: Missing value for 'cgi_extension' at line "
                          << lineNum << std::endl;
                exit(EXIT_FAILURE);
            }
            currentLoc.cgi_extension = val;
        }
        else
        {
            std::cerr << "Error: Unknown or misplaced directive at line " << lineNum
                      << ": \"" << line << "\"" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    file.close();

    if (server.listen == 0)
    {
        std::cerr << "Warning: 'listen' directive not found, using default port 8080" << std::endl;
        server.listen = 8080;
    }

    if (server.root.empty())
    {
        std::cerr << "Warning: 'root' directive not found, using default './www'" << std::endl;
        server.root = "./www";
    }

    return server;
}

std::string ConfigParser::getFilename() const
{
    return filename;
}