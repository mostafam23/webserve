#include "ConfigValidator.hpp"
#include "../utils/Utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <algorithm>

ConfigValidator::ConfigValidator(const std::string &file)
    : filename(file) {}

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
    static const char *valid_methods[] = {"GET", "POST", "DELETE"};
    for (size_t i = 0; i < 3; ++i)
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

bool ConfigValidator::isValidHost(const std::string &host)
{
    if (host == "localhost")
        return true;

    // Check for valid IPv4 format (A.B.C.D)
    int dots = 0;
    for (size_t i = 0; i < host.length(); ++i)
    {
        if (host[i] == '.')
            dots++;
        else if (!ft_isdigit(host[i]))
            return false;
    }
    
    if (dots != 3)
        return false;

    std::istringstream iss(host);
    std::string segment;
    while (ft_getline(iss, segment, '.'))
    {
        if (segment.empty() || segment.length() > 3)
            return false;
        int num = ft_atoi(segment.c_str());
        if (num < 0 || num > 255)
            return false;
    }
    return true;
}

bool ConfigValidator::checkExtraArguments(std::istringstream &iss, const std::string &directiveName, int lineNum)
{
    std::string extraArg;
    if (iss >> extraArg)
    {
        if (extraArg != ";" || (iss >> extraArg))
        {
            printError("'" + directiveName + "' directive has too many arguments", lineNum);
            return false;
        }
    }
    return true;
}

bool ConfigValidator::validate()
{
    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
        printError("Cannot open file '" + filename + "'");
        return false;
    }

    std::string line;
    while (ft_getline(file, line))
    {
        line = removeComments(line);
        lines.push_back(line);
    }

    file.close();

    if (lines.empty())
    {
        printError("Configuration file is empty");
        return false;
    }

    if (!validateBraces())
        return false;

    if (!validateSyntax())
        return false;

    return true;
}

bool ConfigValidator::validateBraces()
{
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

        // Detect server block start
        if (line.find("server") == 0)
        {
            foundServer = true;

            std::string path;
            if (!validateBlockDeclaration(line, "server", i + 1, path))
                return false;

            bool hasBrace = (line.find("{") != std::string::npos);
            i++;

            // If no brace on same line, find next '{'
            if (!hasBrace)
            {
                bool foundBrace = false;

                while (i < lines.size())
                {
                    std::string nextLine = trim(lines[i]);

                    if (nextLine.empty())
                    {
                        i++;
                        continue;
                    }
                    if (nextLine == "{")
                    {
                        foundBrace = true;
                        i++;
                        break;
                    }
                    else if (nextLine.find("{") == 0)
                    {
                        std::string afterBrace = trim(ft_substr(nextLine, 1));
                        if (!afterBrace.empty())
                        {
                            printError("Unexpected text '" + afterBrace + "' after opening brace '{'", i + 1);
                            return false;
                        }
                        foundBrace = true;
                        i++;
                        break;
                    }
                    else
                    {
                        printError("Missing opening brace '{' after 'server' directive", i + 1);
                        return false;
                    }
                }
                if (!foundBrace)
                {
                    printError("Missing opening brace '{' for 'server' block", i);
                    return false;
                }
            }
            if (!validateServerBlock(i))
                return false;
            continue;
        }

        printError("Unexpected directive outside server block: '" + line + "'", i + 1);
        return false;
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

        std::string directive = ft_substr(line, 0, line.find_first_of(" \t"));
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

        if (!value.empty() && value[value.size() - 1] == ';')
        {
            value = ft_substr(value, 0, value.size() - 1);
        }

        int port = 0;
        size_t colonPos = value.find(':');
        if (colonPos != std::string::npos)
        {
            std::string hostPart = ft_substr(value, 0, colonPos);
            if (!isValidHost(hostPart))
            {
                printError("Invalid host in listen directive: '" + hostPart + "'", lineNum);
                return false;
            }
            std::string portStr = ft_substr(value, colonPos + 1);
            port = ft_atoi(portStr.c_str());
        }
        else
        {
            port = ft_atoi(value.c_str());
        }

        if (!isValidPort(port))
        {
            std::ostringstream oss;
            oss << "Port number must be between 1 and 65535 (found: " << port << ")";
            printError(oss.str(), lineNum);
            return false;
        }

        if (!checkExtraArguments(iss, "listen", lineNum))
            return false;
    }
    else if (directive == "host")
    {
        if (inLocation)
        {
            printError("'host' directive not allowed in location block", lineNum);
            return false;
        }
        std::string value;
        if (!(iss >> value))
        {
            printError("'host' directive missing value", lineNum);
            return false;
        }
        if (!value.empty() && value[value.size() - 1] == ';')
        {
            value = ft_substr(value, 0, value.size() - 1);
        }
        if (!isValidHost(value))
        {
            printError("Invalid host: '" + value + "'", lineNum);
            return false;
        }

        if (!checkExtraArguments(iss, "host", lineNum))
            return false;
    }
    else if (directive == "max_size")
    {
        std::string value;
        if (!(iss >> value))
        {
            printError("'max_size' directive missing value", lineNum);
            return false;
        }

        if (!checkExtraArguments(iss, "max_size", lineNum))
            return false;
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
        
        if (!name.empty() && name[name.size() - 1] == ';')
        {
            name = ft_substr(name, 0, name.size() - 1);
        }

        if (!checkExtraArguments(iss, "server_name", lineNum))
            return false;
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
            path = ft_substr(path, 0, path.size() - 1);
        }
        if (!isValidPath(path))
        {
            printError("Invalid path in 'root' directive: '" + path + "'", lineNum);
            return false;
        }

        if (!checkExtraArguments(iss, "root", lineNum))
            return false;
    }
    else if (directive == "index")
    {
        std::string idx;
        if (!(iss >> idx))
        {
            printError("'index' directive missing filename", lineNum);
            return false;
        }

        if (!checkExtraArguments(iss, "index", lineNum))
            return false;
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

        if (!checkExtraArguments(iss, "error_page", lineNum))
            return false;
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
        bool hasEndingSemicolon = false;

        while (iss >> method)
        {
            if (hasEndingSemicolon)
            {
                 printError("'methods' directive has too many arguments after ';'", lineNum);
                 return false;
            }
            if (!method.empty() && method[method.size() - 1] == ';')
            {
                method = ft_substr(method, 0, method.size() - 1);
                hasEndingSemicolon = true;
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
        bool hasExt = false;
        bool hasEndingSemicolon = false;

        while (iss >> ext)
        {
             if (hasEndingSemicolon)
            {
                 printError("'cgi_extension' directive has too many arguments after ';'", lineNum);
                 return false;
            }

            if (!ext.empty() && ext[ext.size() - 1] == ';')
            {
                ext = ft_substr(ext, 0, ext.size() - 1);
                hasEndingSemicolon = true;
            }
            if (!ext.empty())
            {
                if (ext[0] != '.')
                {
                    printError("Invalid extension '" + ext + "' (must start with .)", lineNum);
                    return false;
                }
                hasExt = true;
            }
        }
        if (!hasExt)
        {
            printError("'cgi_extension' directive missing extension", lineNum);
            return false;
        }
    }
    else if (directive == "autoindex")
    {
        if (!inLocation)
        {
            printError("'autoindex' directive only allowed in location block", lineNum);
            return false;
        }
        std::string val;
        if (!(iss >> val))
        {
            printError("'autoindex' directive missing value (on/off)", lineNum);
            return false;
        }
        if (!val.empty() && val[val.size() - 1] == ';')
            val = ft_substr(val, 0, val.size() - 1);
        if (val != "on" && val != "off")
        {
            printError("Invalid value for 'autoindex' (expected on/off)", lineNum);
            return false;
        }

        if (!checkExtraArguments(iss, "autoindex", lineNum))
            return false;
    }
	
    else if (directive != "location" && directive != "server")
    {
        printError("Unknown directive '" + directive + "'", lineNum);
        return false;
    }

    return true;
}

bool ConfigValidator::validateBlockDeclaration(const std::string &line, const std::string &blockType,
                                               int lineNum, std::string &path)
{
    std::string remaining = line;

    if (blockType == "server")
    {
        remaining = ft_substr(line, 6);
    }
    else if (blockType == "location")
    {
        remaining = ft_substr(line, 8);
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
            path = ft_substr(remaining, 0, bracePos);
            path = trim(path);

            std::string afterBrace = ft_substr(remaining, bracePos + 1);
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

            if (path.find_first_of(" \t") != std::string::npos)
            {
                printError("Invalid location path (contains whitespace): '" + path + "'", lineNum);
                return false;
            }
        }
        else
        {
            // No brace on same line
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
            std::string afterBrace = ft_substr(remaining, 1);
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
            std::string garbage = ft_substr(remaining, 0, bracePos);
            garbage = trim(garbage);
            printError("Unexpected text '" + garbage + "' after 'server' directive", lineNum);
            return false;
        }
        else
        {
            // No brace on same line
            path = "";
            return true;
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

    // If no brace on same line, look for it on next non-empty lines
    if (!hasBrace)
    {
        bool foundBrace = false;
        while (idx < lines.size())
        {
            std::string nextLine = trim(lines[idx]);

            if (nextLine.empty())
            {
                idx++;
                continue;
            }

            if (nextLine == "{")
            {
                foundBrace = true;
                idx++;
                break;
            }
            else if (nextLine.find("{") == 0)
            {
                std::string afterBrace = ft_substr(nextLine, 1);
                afterBrace = trim(afterBrace);
                if (!afterBrace.empty())
                {
                    printError("Unexpected text '" + afterBrace + "' after opening brace '{'", idx + 1);
                    return false;
                }
                foundBrace = true;
                idx++;
                break;
            }
            else
            {
                printError("Missing opening brace '{' after location directive", idx + 1);
                return false;
            }
        }
        if (!foundBrace)
        {
            printError("Missing opening brace '{' for location block", idx);
            return false;
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

        std::string directive = ft_substr(line, 0, line.find_first_of(" \t"));
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