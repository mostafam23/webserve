#ifndef CONFIGVALIDATOR_HPP
#define CONFIGVALIDATOR_HPP

#include <string>
#include <vector>
#include <set>

class ConfigValidator {
private:
    std::string filename;
    std::vector<std::string> lines;
    bool debug_mode;

    // Helper functions
    std::string trim(const std::string &str);
    bool isValidPort(int port);
    bool isValidPath(const std::string &path);
    bool isValidMethod(const std::string &method);
    bool isValidErrorCode(int code);
    void printError(const std::string &msg, int lineNum = -1);
    
    // Validation functions
    bool validateSyntax();
    bool validateBraces();
    bool validateServerBlock(size_t &idx);
    bool validateLocationBlock(size_t &idx);
    bool validateDirective(const std::string &line, int lineNum, bool inLocation);
    bool checkDuplicateServerNames();
    bool validateBlockDeclaration(const std::string &line, const std::string &blockType, 
                                   int lineNum, std::string &path);  // ✅ ADD THIS LINE

public:
    ConfigValidator(const std::string &filename, bool debug = false);
    bool validate();
};

#endif