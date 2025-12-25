#include "ArgValidation.hpp"
static bool endsWith(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    size_t start = s.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (s[start + i] != suffix[i]) return false;
    }
    return true;
}

bool validateArgs(const std::vector<std::string>& args, std::string& error) {
    if (args.empty()) {
        error = "missing configuration file argument";
        return false;
    }
    if (args.size() > 1) {
        error = "too many arguments";
        return false;
    }
    if (args.size() == 1) {
        const std::string& path = args[0];
        if (!endsWith(path, ".conf")) {
            error = "config file must end with .conf";
            return false;
        }
    }
    return true;
}
