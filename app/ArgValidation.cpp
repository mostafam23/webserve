#include "ArgValidation.hpp"

static bool endsWith(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool ArgCountHandler::handle(const std::vector<std::string>& args, std::string& error) {
    if (args.size() <= 1) return true;
    error = "too many arguments";
    return false;
}

bool ConfExtensionHandler::handle(const std::vector<std::string>& args, std::string& error) {
    if (args.size() == 0) return true;
    const std::string& path = args[0];
    if (!endsWith(path, ".conf")) {
        error = "config file must end with .conf";
        return false;
    }
    return true;
}
