#ifndef PATH_UTILS_HPP
#define PATH_UTILS_HPP

#include <string>
#include <climits>
#include <vector>
#include <iostream>

bool isDirectory(const std::string &path);
std::string generateDirectoryListing(const std::string &path, const std::string &requestPath);
long parseSize(const std::string &sizeStr);
std::string trim(const std::string &str);
std::string removeComments(const std::string &line);
int ft_tolower(int c);
int ft_isdigit(int c);
int ft_strncmp(const char *s1, const char *s2, size_t n);
std::istream &ft_getline(std::istream &is, std::string &str);
std::istream &ft_getline(std::istream &is, std::string &str, char delim);
std::string ft_substr(const std::string &str, size_t pos, size_t len = std::string::npos);

// Additional utility functions (replacements for non-allowed functions)
int ft_atoi(const char *str);
long ft_atol(const char *str);
long ft_strtol(const char *str, char **endptr, int base);
unsigned long ft_strtoul(const char *str, char **endptr, int base);
void ft_memset(void *s, int c, size_t n);
void ft_strcpy(char *dest, const char *src);
int ft_remove(const char *filename);

// File I/O wrappers (replacements for stdio functions not in allowed list)
int ft_file_open_temp(char *template_path);
int ft_file_close(int fd);

// Error handling (replacement for perror which is not in allowed list)
void ft_perror(const char *msg);

#endif // PATH_UTILS_HPP
