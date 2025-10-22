#ifndef CONFIGSTRUCTS_HPP
#define CONFIGSTRUCTS_HPP

#include <string>
#include <map>
#include <set>

struct Location {
    std::string path;
    std::set<std::string> methods;
    std::string root;
    std::string cgi_extension;

    Location() {
        path = "";
        root = "";
        cgi_extension = "";
    }
};

struct Server {
    int listen;
    std::string host;
    std::string max_size;
    std::string server_name;
    std::string root;
    std::string index;
    std::map<int, std::string> error_pages;
    Location locations[10];
    int location_count;

    Server() {
        listen = 8080;
        host = "127.0.0.1";
        max_size = "1b";
        server_name = "localhost";
        root = "./www";
        index = "index.html";
        location_count = 0;
    }
};

#endif
