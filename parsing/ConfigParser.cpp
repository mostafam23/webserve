#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib> // for atoi

// Constructor
ConfigParser::ConfigParser(const std::string &file) {
    this->filename = file;
}

// Helper: trim whitespace
std::string ConfigParser::trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t");
    size_t last = str.find_last_not_of(" \t");
    if (first == std::string::npos) return "";
    return str.substr(first, last - first + 1);
}

// Helper: get value after key and remove trailing ';' or '{'
std::string ConfigParser::getValue(const std::string &line) {
    size_t pos = line.find(' ');
    if (pos == std::string::npos) return "";
    std::string val = line.substr(pos + 1);
    val = trim(val);

    // Remove trailing ';'
    if (!val.empty() && val[val.length() - 1] == ';')
        val = val.substr(0, val.length() - 1);

    // Remove trailing '{' (for location paths)
    if (!val.empty() && val[val.length() - 1] == '{')
        val = trim(val.substr(0, val.length() - 1));

    return trim(val);
}

// parseServer
Server ConfigParser::parseServer() {
    Server server;
    server.location_count = 0;

    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "Cannot open config file: " << filename << std::endl;
        return server;
    }

    std::string line;
    Location currentLoc;
    bool inLocation = false;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.find("listen") == 0) {
            server.listen = atoi(getValue(line).c_str());
        } else if (line.find("server_name") == 0) {
            server.server_name = getValue(line);
        } else if (line.find("root") == 0 && !inLocation) {
            server.root = getValue(line);
        } else if (line.find("index") == 0) {
            server.index = getValue(line);
        } else if (line.find("error_page") == 0) {
            std::istringstream iss(line);
            std::string key;
            int code;
            std::string value;
            iss >> key >> code >> value;

            // Remove trailing ';' if exists
            if (!value.empty() && value[value.size() - 1] == ';')
                value = value.substr(0, value.size() - 1);

            server.error_pages[code] = value;
        } else if (line.find("location") == 0) {
            inLocation = true;
            currentLoc = Location();
            currentLoc.path = getValue(line);
        } else if (inLocation && line.find("methods") == 0) {
            std::string methods_line = getValue(line);
            std::istringstream iss(methods_line);
            std::string method;
            while (iss >> method)
                currentLoc.methods.insert(method);
        } else if (inLocation && line.find("root") == 0) {
            currentLoc.root = getValue(line);
        } else if (inLocation && line.find("cgi_extension") == 0) {
            currentLoc.cgi_extension = getValue(line);
        } else if (line.find("}") != std::string::npos && inLocation) {
            server.locations[server.location_count++] = currentLoc;
            inLocation = false;
        }
    }

    return server;
}

// getFilename
std::string ConfigParser::getFilename() const {
    return filename;
}
