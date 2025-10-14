#include "parsing/ConfigParser.hpp"
#include <iostream>
#include <set>
#include <map>

int main() {
    // Create parser with your config file
    ConfigParser parser("webserv.conf");

    // Parse the config
    Server server = parser.parseServer();

    // Print server info
    std::cout << "Server listening on port: " << server.listen << std::endl;
    std::cout << "Server name: " << server.server_name << std::endl;
    std::cout << "Root folder: " << server.root << std::endl;
    std::cout << "Index file: " << server.index << std::endl;

    // Print error pages
    std::cout << "Error pages:" << std::endl;
    std::map<int, std::string>::iterator it;
    for (it = server.error_pages.begin(); it != server.error_pages.end(); ++it) {
        std::cout << "  " << it->first << " -> " << it->second << std::endl;
    }

    // Print locations
    std::cout << "Locations:" << std::endl;
    for (int i = 0; i < server.location_count; ++i) {
        std::cout << "  Path: " << server.locations[i].path << std::endl;

        std::cout << "  Methods: ";
        std::set<std::string>::iterator mit;
        for (mit = server.locations[i].methods.begin(); mit != server.locations[i].methods.end(); ++mit) {
            std::cout << *mit << " ";
        }
        std::cout << std::endl;

        if (!server.locations[i].root.empty())
            std::cout << "  Root: " << server.locations[i].root << std::endl;
        if (!server.locations[i].cgi_extension.empty())
            std::cout << "  CGI Extension: " << server.locations[i].cgi_extension << std::endl;

        std::cout << std::endl;
    }

    return 0;
}
