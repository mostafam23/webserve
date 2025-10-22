#include "ConfigParser.hpp"

ConfigParser::ConfigParser(const std::string &file) {
    this->filename = file;
}

bool ConfigParser::validateConfigFile(const std::string &filename, bool debug) {
    ConfigValidator validator(filename, debug);
    return validator.validate();
}

std::string ConfigParser::trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return str.substr(first, last - first + 1);
}

std::string ConfigParser::getValue(const std::string &line) {
    // Find first space or tab after the directive name
    size_t pos = line.find_first_of(" \t");
    if (pos == std::string::npos) return "";
    
    // Skip all whitespace after the directive name
    size_t start = line.find_first_not_of(" \t", pos);
    if (start == std::string::npos) return "";
    
    std::string val = line.substr(start);
    val = trim(val);

    // Remove trailing semicolon
    if (!val.empty() && val[val.length() - 1] == ';')
        val = val.substr(0, val.length() - 1);

    // Remove trailing opening brace
    if (!val.empty() && val[val.length() - 1] == '{')
        val = val.substr(0, val.length() - 1);

    return trim(val);
}

Server ConfigParser::parseServer() {
    Server server;

    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        return server;
    }

    std::string line;
    Location currentLoc;
    bool inLocation = false;
    bool inServer = false;

    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') 
            continue;

        // Detect server block start
        if (line.find("server") == 0) {
            inServer = true;
            continue;
        }

        // Skip lines until we're inside a server block
        if (!inServer)
            continue;

        // Handle closing braces
        if (line == "}" || line.find("}") == 0) {
            if (inLocation) {
                // End of location block
                if (server.location_count < 10) {
                    server.locations[server.location_count++] = currentLoc;
                } else {
                    std::cerr << "Warning: Maximum 10 locations supported. Ignoring location: " 
                              << currentLoc.path << std::endl;
                }
                inLocation = false;
            } else if (inServer) {
                // End of server block
                inServer = false;
                break;
            }
            continue;
        }

        // Parse server-level directives
        if (line.find("listen") == 0) {
            std::string val = getValue(line);
            if (!val.empty()) {
                server.listen = atoi(val.c_str());
                if (server.listen <= 0 || server.listen > 65535) {
                    std::cerr << "Warning: Invalid port number " << server.listen 
                              << ", using default 8080" << std::endl;
                    server.listen = 8080;
                }
            }
        } 
        else if (line.find("server_name") == 0) {
            std::string val = getValue(line);
            if (!val.empty())
                server.server_name = val;
        } 
        else if (line.find("root") == 0 && !inLocation) {
            std::string val = getValue(line);
            if (!val.empty())
                server.root = val;
        } 
        else if (line.find("index") == 0 && !inLocation) {
            std::string val = getValue(line);
            if (!val.empty())
                server.index = val;
        } 
        else if (line.find("error_page") == 0) {
            std::istringstream iss(line);
            std::string key;
            int code;
            std::string value;
            iss >> key >> code >> value;

            if (value.empty()) {
                std::cerr << "Warning: error_page missing path for code " << code << std::endl;
                continue;
            }

            // Remove trailing semicolon
            if (!value.empty() && value[value.size() - 1] == ';')
                value = value.substr(0, value.size() - 1);

            server.error_pages[code] = value;
        } 
        else if (line.find("location") == 0) {
            // Start a new location block
            inLocation = true;
            currentLoc = Location(); // Reset to default-initialized Location
            std::string path = getValue(line);
            if (!path.empty())
                currentLoc.path = path;
            else {
                std::cerr << "Warning: location directive missing path" << std::endl;
                inLocation = false;
            }
        } 
        else if (inLocation && line.find("methods") == 0) {
            std::string methods_line = getValue(line);
            std::istringstream iss(methods_line);
            std::string method;
            while (iss >> method) {
                // Remove any trailing semicolon from individual methods
                if (!method.empty() && method[method.size() - 1] == ';')
                    method = method.substr(0, method.size() - 1);
                if (!method.empty())
                    currentLoc.methods.insert(method);
            }
        } 
        else if (inLocation && line.find("root") == 0) {
            std::string val = getValue(line);
            if (!val.empty())
                currentLoc.root = val;
        } 
        else if (inLocation && line.find("cgi_extension") == 0) {
            std::string val = getValue(line);
            if (!val.empty())
                currentLoc.cgi_extension = val;
        }
    }

    file.close();

    // Validation: Check if required fields are set
    if (server.listen == 0) {
        std::cerr << "Warning: 'listen' directive not found, using default port 8080" << std::endl;
        server.listen = 8080;
    }

    if (server.root.empty()) {
        std::cerr << "Warning: 'root' directive not found, using default './www'" << std::endl;
        server.root = "./www";
    }

    return server;
}

std::string ConfigParser::getFilename() const {
    return filename;
}