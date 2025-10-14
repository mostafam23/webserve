#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "ConfigStructs.hpp"
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
};

#endif
