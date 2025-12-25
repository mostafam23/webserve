// Null Object Pattern: Provide a default config source when no .conf is specified.
#ifndef CONFIG_SOURCE_HPP
#define CONFIG_SOURCE_HPP

#include <string>
#include <stdexcept>
#include "../parsing_validation/ConfigStructs.hpp"
#include "../parsing_validation/ConfigParser.hpp"

struct IConfigSource {
    virtual ~IConfigSource() {}
    virtual Servers buildServers() = 0;
    // Backward compatibility method
    virtual Server buildServer() = 0;
};

struct DefaultConfigSource : public IConfigSource {
    Servers buildServers() { 
        Servers servers;
        servers.addServer(Server());
        return servers;
    }
    Server buildServer() { return Server(); }
};

struct FileConfigSource : public IConfigSource {
    FileConfigSource(const std::string& path) : path_(path) {}
    Servers buildServers() {
        if (!ConfigParser::validateConfigFile(path_)) {
            throw std::runtime_error("Configuration validation failed");
        }
        ConfigParser parser(path_);
        return parser.parseServers();
    }
    Server buildServer() {
        if (!ConfigParser::validateConfigFile(path_)) {
            throw std::runtime_error("Configuration validation failed");
        }
        ConfigParser parser(path_);
        return parser.parseServer();
    }
private:
    std::string path_;
};

#endif
