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
    std::string getValue(const std::string &line);
    bool lineRequiresSemicolon(const std::string &line);  // ✅ NEW: check if a line must end with ';'

public:
    // ===== Constructors =====
    ConfigParser(const std::string &filename);

    // ===== Core Functions =====
    Servers parseServers();              // parses multiple server blocks

    // ===== Static Validation =====
    static bool validateConfigFile(const std::string &filename);
};

#endif
