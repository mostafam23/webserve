// Simple CLI argument validation utilities.
#ifndef ARG_VALIDATION_HPP
#define ARG_VALIDATION_HPP

#include <string>
#include <vector>

// Validate arguments for this application.
// Rules:
// - Allow 0 or 1 argument.
// - If provided, the single argument must end with ".conf".
bool validateArgs(const std::vector<std::string>& args, std::string& error);

#endif
