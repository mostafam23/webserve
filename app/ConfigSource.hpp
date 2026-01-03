// Null Object Pattern: Provide a default config source when no .conf is specified.
#ifndef CONFIG_SOURCE_HPP
#define CONFIG_SOURCE_HPP

#include <string>
#include <stdexcept>
#include "../parsing_validation/ConfigStructs.hpp"
#include "../parsing_validation/ConfigParser.hpp"


struct FileConfigSource
{


    FileConfigSource(const std::string& path) : path_(path) {}

    Servers buildServers() 
    {
        if (!ConfigParser::validateConfigFile(path_)) {
            throw std::runtime_error("Configuration validation failed");
        }
        ConfigParser parser(path_);
        return parser.parseServers();
    }

    private:
        std::string path_;

};

#endif
