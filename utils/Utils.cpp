#include "Utils.hpp"
#include <sys/stat.h>

bool isDirectory(const std::string &path) {
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return S_ISDIR(s.st_mode);
    return false;
}

int ft_atoi(const char* str) {
    if (!str) return 0;

    int i = 0;
    // Skip leading whitespaces
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n')
        i++;

    // Handle optional sign
    int sign = 1;
    if (str[i] == '+' || str[i] == '-') {
        if (str[i] == '-') sign = -1;
        i++;
    }

    int result = 0;
    while (str[i] >= '0' && str[i] <= '9') {
        int digit = str[i] - '0';

        // Check for overflow
        if (result > (INT_MAX - digit) / 10)
            return (sign == 1) ? INT_MAX : INT_MIN;

        result = result * 10 + digit;
        i++;
    }

    return result * sign;
}