#include "ConfigValidator.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <algorithm>

ConfigValidator::ConfigValidator(const std::string &file, bool debug)
    : filename(file), debug_mode(debug) {}

std::string ConfigValidator::trim(const std::string &str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    return str.substr(first, last - first + 1);
}

// ✅ Helper to remove inline comments
static std::string removeComments(const std::string &line)
{
    size_t pos = line.find('#');
    if (pos != std::string::npos)
        return line.substr(0, pos);
    return line;
}

void ConfigValidator::printError(const std::string &msg, int lineNum)
{
    std::cerr << "Error: Invalid configuration file - " << msg;
    if (lineNum >= 0)
    {
        std::cerr << " (line " << lineNum << ")";
    }
    std::cerr << std::endl;
}

bool ConfigValidator::isValidPort(int port)
{
    return port >= 1 && port <= 65535;
}

bool ConfigValidator::isValidPath(const std::string &path)
{
    if (path.empty())
        return false;
    if (path.find("..") != std::string::npos)
        return false;
    return true;
}

bool ConfigValidator::isValidMethod(const std::string &method)
{
    static const char *valid_methods[] = {"GET", "POST", "DELETE", "PUT", "HEAD", "OPTIONS"};
    for (size_t i = 0; i < 6; ++i)
    {
        if (method == valid_methods[i])
            return true;
    }
    return false;
}

bool ConfigValidator::isValidErrorCode(int code)
{
    return code >= 100 && code <= 599;
}

bool ConfigValidator::validate()
{
    if (debug_mode)
    {
        std::cout << "[DEBUG] Starting validation of: " << filename << std::endl;
    }

    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
        printError("Cannot open file '" + filename + "'");
        exit(EXIT_FAILURE);
    }

    std::string line;
    while (std::getline(file, line))
    {
        line = removeComments(line);
        lines.push_back(line);
    }
    file.close();

    if (lines.empty())
    {
        printError("Configuration file is empty");
        exit(EXIT_FAILURE);
    }

    if (debug_mode)
    {
        std::cout << "[DEBUG] File loaded. Total lines: " << lines.size() << std::endl;
    }

    if (!validateBraces())
    {
        exit(EXIT_FAILURE);
    }

    if (!validateSyntax())
    {
        exit(EXIT_FAILURE);
    }

    if (!checkDuplicateServerNames())
    {
        exit(EXIT_FAILURE);
    }

    if (debug_mode)
    {
        std::cout << "[DEBUG] Validation completed successfully" << std::endl;
    }

    return true;
}

bool ConfigValidator::validateBraces()
{
    if (debug_mode)
    {
        std::cout << "[DEBUG] Validating braces matching..." << std::endl;
    }

    int braceCount = 0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string line = trim(lines[i]);
        if (line.empty())
            continue;

        for (size_t j = 0; j < line.length(); ++j)
        {
            if (line[j] == '{')
                braceCount++;
            if (line[j] == '}')
                braceCount--;

            if (braceCount < 0)
            {
                printError("Unmatched closing brace '}'", i + 1);
                return false;
            }
        }
    }

    if (braceCount > 0)
    {
        printError("Missing closing brace '}' - unclosed block");
        return false;
    }
    else if (braceCount < 0)
    {
        printError("Extra closing brace '}'");
        return false;
    }

    return true;
}

bool ConfigValidator::validateSyntax()
{
    if (debug_mode)
    {
        std::cout << "[DEBUG] Validating syntax and directives..." << std::endl;
    }

    bool foundServer = false;
    size_t i = 0;

    while (i < lines.size())
    {
        std::string line = trim(lines[i]);

        if (line.empty())
        {
            i++;
            continue;
        }

        if (line.find("server") == 0)
        {
            foundServer = true;

            std::string path;
            if (!validateBlockDeclaration(line, "server", i + 1, path))
            {
                return false;
            }

            bool hasBrace = line.find("{") != std::string::npos;
            i++;

            if (!hasBrace)
            {
                if (i < lines.size())
                {
                    std::string nextLine = trim(lines[i]);
                    if (nextLine != "{")
                    {
                        if (nextLine.find("{") == 0)
                        {
                            std::string afterBrace = nextLine.substr(1);
                            afterBrace = trim(afterBrace);
                            if (!afterBrace.empty())
                            {
                                printError("Unexpected text '" + afterBrace + "' after opening brace '{'", i + 1);
                                return false;
                            }
                        }
                        else
                        {
                            printError("Missing opening brace '{' after 'server' directive", i + 1);
                            return false;
                        }
                    }
                    i++;
                }
            }

            if (!validateServerBlock(i))
            {
                return false;
            }
        }
        else
        {
            printError("Unexpected directive outside server block: '" + line + "'", i + 1);
            return false;
        }
    }

    if (!foundServer)
    {
        printError("No 'server' block found in configuration file");
        return false;
    }

    return true;
}

bool ConfigValidator::validateServerBlock(size_t &idx)
{
    std::set<std::string> serverDirectives;
    bool hasListen = false;
    bool hasRoot = false;

    while (idx < lines.size())
    {
        std::string line = trim(lines[idx]);

        if (line.empty())
        {
            idx++;
            continue;
        }

        if (line == "}" || line.find("}") == 0)
        {
            idx++;
            break;
        }

        if (line.find("location") == 0)
        {
            if (!validateLocationBlock(idx))
            {
                return false;
            }
            continue;
        }

        if (line.find("listen") == 0)
        {
            hasListen = true;
        }
        if (line.find("root") == 0)
        {
            hasRoot = true;
        }

        if (!validateDirective(line, idx + 1, false))
        {
            return false;
        }

        std::string directive = line.substr(0, line.find_first_of(" \t"));
        if (directive != "error_page")
        {
            if (serverDirectives.count(directive))
            {
                printError("Duplicate directive '" + directive + "'", idx + 1);
                return false;
            }
            serverDirectives.insert(directive);
        }

        idx++;
    }

    if (!hasListen)
    {
        printError("Missing required 'listen' directive in server block");
        return false;
    }

    if (!hasRoot)
    {
        printError("Missing required 'root' directive in server block");
        return false;
    }

    return true;
}

bool ConfigValidator::validateDirective(const std::string &line, int lineNum, bool inLocation)
{
    std::istringstream iss(line);
    std::string directive;
    iss >> directive;

    if (directive == "listen")
    {
        if (inLocation)
        {
            printError("'listen' directive not allowed in location block", lineNum);
            return false;
        }
        std::string value;
        if (!(iss >> value))
        {
            printError("'listen' directive missing value", lineNum);
            return false;
        }
        
        // Remove semicolon
        if (!value.empty() && value[value.size() - 1] == ';')
        {
            value = value.substr(0, value.size() - 1);
        }
        
        // Parse host:port or just port
        int port = 0;
        size_t colonPos = value.find(':');
        if (colonPos != std::string::npos)
        {
            // Format: host:port
            std::string portStr = value.substr(colonPos + 1);
            port = atoi(portStr.c_str());
        }
        else
        {
            // Format: just port
            port = atoi(value.c_str());
        }
        
        if (!isValidPort(port))
        {
            std::ostringstream oss;
            oss << "Port number must be between 1 and 65535 (found: " << port << ")";
            printError(oss.str(), lineNum);
            return false;
        }
    }
    else if (directive == "max_size")
    {
        // Accept max_size directive
        std::string value;
        if (!(iss >> value))
        {
            printError("'max_size' directive missing value", lineNum);
            return false;
        }
    
    }
    else if (directive == "server_name")
    {
        if (inLocation)
        {
            printError("'server_name' directive not allowed in location block", lineNum);
            return false;
        }
        std::string name;
        if (!(iss >> name))
        {
            printError("'server_name' directive missing value", lineNum);
            return false;
        }
    }
    else if (directive == "root")
    {
        std::string path;
        if (!(iss >> path))
        {
            printError("'root' directive missing path", lineNum);
            return false;
        }
        if (!path.empty() && path[path.size() - 1] == ';')
        {
            path = path.substr(0, path.size() - 1);
        }
        if (!isValidPath(path))
        {
            printError("Invalid path in 'root' directive: '" + path + "'", lineNum);
            return false;
        }
    }
    else if (directive == "index")
    {
        if (inLocation)
        {
            printError("'index' directive not allowed in location block", lineNum);
            return false;
        }
        std::string idx;
        if (!(iss >> idx))
        {
            printError("'index' directive missing filename", lineNum);
            return false;
        }
    }
    else if (directive == "error_page")
    {
        if (inLocation)
        {
            printError("'error_page' directive not allowed in location block", lineNum);
            return false;
        }
        int code;
        std::string path;
        if (!(iss >> code >> path))
        {
            printError("'error_page' directive requires error code and path", lineNum);
            return false;
        }
        if (!isValidErrorCode(code))
        {
            printError("Invalid HTTP error code in 'error_page' directive", lineNum);
            return false;
        }
    }
    else if (directive == "methods")
    {
        if (!inLocation)
        {
            printError("'methods' directive only allowed in location block", lineNum);
            return false;
        }
        std::string method;
        bool hasMethod = false;
        while (iss >> method)
        {
            if (!method.empty() && method[method.size() - 1] == ';')
            {
                method = method.substr(0, method.size() - 1);
            }
            if (!method.empty())
            {
                if (!isValidMethod(method))
                {
                    printError("Invalid HTTP method '" + method + "' in 'methods' directive", lineNum);
                    return false;
                }
                hasMethod = true;
            }
        }
        if (!hasMethod)
        {
            printError("'methods' directive missing HTTP methods", lineNum);
            return false;
        }
    }
    else if (directive == "cgi_extension")
    {
        if (!inLocation)
        {
            printError("'cgi_extension' directive only allowed in location block", lineNum);
            return false;
        }
        std::string ext;
        if (!(iss >> ext))
        {
            printError("'cgi_extension' directive missing extension", lineNum);
            return false;
        }
    }
    else if (directive != "location" && directive != "server")
    {
        printError("Unknown directive '" + directive + "'", lineNum);
        return false;
    }

    return true;
}

bool ConfigValidator::checkDuplicateServerNames()
{
    if (debug_mode)
    {
        std::cout << "[DEBUG] Checking for duplicate server names..." << std::endl;
    }

    std::set<std::string> serverNames;
    bool inServer = false;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string line = trim(lines[i]);

        if (line.empty())
            continue;

        if (line.find("server") == 0 && line.find("server_name") != 0)
        {
            inServer = true;
        }

        if (inServer && line.find("server_name") == 0)
        {
            std::istringstream iss(line);
            std::string directive, name;
            iss >> directive >> name;

            if (!name.empty() && name[name.size() - 1] == ';')
            {
                name = name.substr(0, name.size() - 1);
            }

            if (serverNames.count(name))
            {
                printError("Duplicate server_name '" + name + "'", i + 1);
                return false;
            }
            serverNames.insert(name);
        }

        if (line == "}" && inServer)
        {
            inServer = false;
        }
    }

    return true;
}

bool ConfigValidator::validateBlockDeclaration(const std::string &line, const std::string &blockType,
                                               int lineNum, std::string &path)
{
    std::string remaining = line;

    if (blockType == "server")
    {
        remaining = line.substr(6);
    }
    else if (blockType == "location")
    {
        remaining = line.substr(8);
    }

    remaining = trim(remaining);

    if (remaining.empty())
    {
        if (blockType == "location")
        {
            printError("Location directive missing path", lineNum);
            return false;
        }
        path = "";
        return true;
    }

    if (remaining == "{")
    {
        if (blockType == "location")
        {
            printError("Location directive missing path", lineNum);
            return false;
        }
        path = "";
        return true;
    }

    if (blockType == "location")
    {
        size_t bracePos = remaining.find('{');

        if (bracePos != std::string::npos)
        {
            path = remaining.substr(0, bracePos);
            path = trim(path);

            std::string afterBrace = remaining.substr(bracePos + 1);
            afterBrace = trim(afterBrace);

            if (!afterBrace.empty())
            {
                printError("Unexpected text '" + afterBrace + "' after opening brace '{'", lineNum);
                return false;
            }

            if (path.empty())
            {
                printError("Location directive missing path", lineNum);
                return false;
            }
        }
        else
        {
            path = remaining;

            if (path.find_first_of(" \t") != std::string::npos)
            {
                printError("Invalid location path (contains whitespace): '" + path + "'", lineNum);
                return false;
            }
        }

        return true;
    }

    if (blockType == "server")
    {
        size_t bracePos = remaining.find('{');

        if (bracePos == 0)
        {
            std::string afterBrace = remaining.substr(1);
            afterBrace = trim(afterBrace);

            if (!afterBrace.empty())
            {
                printError("Unexpected text '" + afterBrace + "' after opening brace '{'", lineNum);
                return false;
            }
            path = "";
            return true;
        }
        else if (bracePos != std::string::npos)
        {
            std::string garbage = remaining.substr(0, bracePos);
            garbage = trim(garbage);
            printError("Unexpected text '" + garbage + "' after 'server' directive", lineNum);
            return false;
        }
        else
        {
            printError("Unexpected text '" + remaining + "' after 'server' directive", lineNum);
            return false;
        }
    }

    return false;
}

bool ConfigValidator::validateLocationBlock(size_t &idx)
{
    std::string line = trim(lines[idx]);

    std::string path;
    if (!validateBlockDeclaration(line, "location", idx + 1, path))
    {
        return false;
    }

    bool hasBrace = line.find('{') != std::string::npos;
    idx++;

    if (!hasBrace)
    {
        if (idx < lines.size())
        {
            std::string nextLine = trim(lines[idx]);
            if (nextLine != "{")
            {
                if (nextLine.find("{") == 0)
                {
                    std::string afterBrace = nextLine.substr(1);
                    afterBrace = trim(afterBrace);
                    if (!afterBrace.empty())
                    {
                        printError("Unexpected text '" + afterBrace + "' after opening brace '{'", idx + 1);
                        return false;
                    }
                }
                else
                {
                    printError("Missing opening brace '{' after location directive", idx + 1);
                    return false;
                }
            }
            idx++;
        }
    }

    std::set<std::string> locationDirectives;

    while (idx < lines.size())
    {
        line = trim(lines[idx]);

        if (line.empty())
        {
            idx++;
            continue;
        }

        if (line == "}" || line.find("}") == 0)
        {
            idx++;
            break;
        }

        if (!validateDirective(line, idx + 1, true))
        {
            return false;
        }

        std::string directive = line.substr(0, line.find_first_of(" \t"));
        if (locationDirectives.count(directive))
        {
            printError("Duplicate directive '" + directive + "' in location block", idx + 1);
            return false;
        }
        locationDirectives.insert(directive);

        idx++;
    }

    return true;
}