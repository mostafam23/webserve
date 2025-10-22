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

    std::string trim(const std::string &str);
    std::string getValue(const std::string &line);

public:
    ConfigParser(const std::string &filename);
    Server parseServer();
    std::string getFilename() const;
    
    // Validates config file before parsing
    static bool validateConfigFile(const std::string &filename, bool debug = false);
};

#endif