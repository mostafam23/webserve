// Intentionally left minimal — implementations moved to ConfigParser_Parse.cpp
#include "ConfigParser.hpp"
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

std::string cp_removeComments(const std::string &line)
{
    size_t pos = line.find('#');
    if (pos != std::string::npos)
        return line.substr(0, pos);
    return line;
}

// helpers are implemented in ConfigParser_Utils.cpp and cp_removeComments() in ConfigParser_Internal.cpp

Server ConfigParser::parseServer()
{
    Server server;

    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        throwError("Cannot open config file", 0);
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
                throwError("Unexpected text after 'server'", lineNum);
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
            throwError("Missing semicolon", lineNum);
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
                    throwError("Invalid port number", lineNum);
                }
            }
            else
            {
                throwError("Missing value for 'listen'", lineNum);
            }
        }
        else if (line.find("host") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'host'", lineNum);
            }
            server.host = val;
        }
        else if (line.find("max_size") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'max_size'", lineNum);
            }
            server.max_size = val;
        }
        else if (line.find("server_name") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'server_name'", lineNum);
            }
            server.server_name = val;
        }
        else if (line.find("root") == 0 && !inLocation)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'root'", lineNum);
            }
            server.root = val;
        }
        else if (line.find("index") == 0 && !inLocation)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'index'", lineNum);
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
                throwError("Missing value for 'error_page'", lineNum);
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
                throwError("Missing path for 'location'", lineNum);
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
                    throwError("Expected '{' after 'location'", lineNum);
                }
            }

            if (!foundBrace)
            {
                throwError("Expected '{' after 'location' but reached end of file", lineNum);
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
                throwError("Missing value for 'root' in location block", lineNum);
            }
            currentLoc.root = val;
        }
        else if (inLocation && line.find("index") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'index' in location block", lineNum);
            }
            currentLoc.index = val;
        }
        else if (inLocation && line.find("autoindex") == 0)
        {
            std::string val = getValue(line);
            if (val == "on")
                currentLoc.autoindex = true;
            else if (val == "off")
                currentLoc.autoindex = false;
            else
            {
                throwError("Invalid value for 'autoindex'. Expected 'on' or 'off'", lineNum);
            }
        }
        else if (inLocation && line.find("return") == 0)
        {
            std::istringstream iss(line);
            std::string key;
            int code;
            std::string url;
            iss >> key >> code >> url;

            if (url.empty())
            {
                throwError("Invalid return directive", lineNum);
            }
            if (!url.empty() && url[url.size() - 1] == ';')
                url = url.substr(0, url.size() - 1);

            currentLoc.redirect = std::make_pair(code, url);
        }
        else if (inLocation && line.find("cgi_extension") == 0)
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
                    ext = ext.substr(0, ext.size() - 1);
                currentLoc.cgi_extensions.push_back(ext);
            }
        }
        else if (inLocation && line.find("upload_path") == 0)
        {
            std::string val = getValue(line);
            if (val.empty())
            {
                throwError("Missing value for 'upload_path'", lineNum);
            }
            currentLoc.upload_path = val;
            std::cout << "[DEBUG] Parsed upload_path: " << val << " for location: " << currentLoc.path << std::endl;
        }
        else
        {
            throwError("Unknown or misplaced directive", lineNum);
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
        {
            continue;
        }

        if (!inServer)
            continue;

        // Check for semicolons
        if (lineRequiresSemicolon(line) && line[line.size() - 1] != ';')
        {
            throwError("Missing semicolon", lineNum);
        }

        if (line == "}" || line.find("}") == 0)
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
                // Add completed server to servers collection
                if (currentServer.listen <= 0 || currentServer.listen > 65535)
                {
                    std::cerr << "Warning: Invalid port number for server, using default 8080" << std::endl;
                    currentServer.listen = 8080;
                }
                if (currentServer.root.empty())
                {
                    std::cerr << "Warning: 'root' directive not found for server, using default './www'" << std::endl;
                    currentServer.root = "./www";
                }
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
                    currentServer.host = val.substr(0, colonPos);
                    std::string portStr = val.substr(colonPos + 1);
                    currentServer.listen = atoi(portStr.c_str());
                }
                else
                {
                    currentServer.listen = atoi(val.c_str());
                }

                if (currentServer.listen <= 0 || currentServer.listen > 65535)
                {
                    throwError("Invalid port number", lineNum);
                }
            }
            else
            {
                throwError("Missing value for 'listen'", lineNum);
            }
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
                int errorCode = atoi(val.substr(0, spacePos).c_str());
                std::string errorPath = val.substr(spacePos + 1);
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
                        ext = ext.substr(0, ext.size() - 1);
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
                    url = url.substr(0, url.size() - 1);
                currentLoc.redirect = std::make_pair(code, url);
            }
        }
    }

    // Handle case where file ends without closing server block
    if (inServer)
    {
        std::cerr << "Warning: Unclosed server block at end of file" << std::endl;
        if (currentServer.listen <= 0 || currentServer.listen > 65535)
        {
            std::cerr << "Warning: Invalid port number for server, using default 8080" << std::endl;
            currentServer.listen = 8080;
        }
        if (currentServer.root.empty())
        {
            std::cerr << "Warning: 'root' directive not found for server, using default './www'" << std::endl;
            currentServer.root = "./www";
        }
        servers.addServer(currentServer);
    }

    // If no servers were found, create a default server
    if (servers.empty())
    {
        std::cerr << "Warning: No server blocks found in config file, using default server" << std::endl;
        servers.addServer(Server());
    }

    return servers;
}

std::string ConfigParser::getFilename() const
{
    return filename;
}