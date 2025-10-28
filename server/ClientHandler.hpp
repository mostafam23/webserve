#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <string>

struct ClientData {
    int socket;
    std::string root;
    std::string index;

    ClientData(int s, const std::string &r, const std::string &i)
        : socket(s), root(r), index(i) {}
};

void handleClient(int client_sock, const std::string &root, const std::string &index);

#endif // CLIENT_HANDLER_HPP
