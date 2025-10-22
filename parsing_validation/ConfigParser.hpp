#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "ConfigStructs.hpp"
#include "ConfigValidator.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

class ConfigParser {
private:
    std::string filename;

    // ===== Helper functions =====
    std::string trim(const std::string &str);
    std::string getValue(const std::string &line);
    bool lineRequiresSemicolon(const std::string &line);  // ✅ NEW: check if a line must end with ';'

public:
    // ===== Constructors =====
    ConfigParser(const std::string &filename);

    // ===== Core Functions =====
    Server parseServer();                // parses and builds Server struct
    std::string getFilename() const;     // returns config filename

    // ===== Static Validation =====
    static bool validateConfigFile(const std::string &filename, bool debug = false);
};

#endif
