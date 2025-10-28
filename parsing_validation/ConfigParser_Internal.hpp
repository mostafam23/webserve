#ifndef CONFIGPARSER_INTERNAL_HPP
#define CONFIGPARSER_INTERNAL_HPP

#include <string>

// Internal helper shared across parser units
std::string cp_removeComments(const std::string &line);

#endif // CONFIGPARSER_INTERNAL_HPP
