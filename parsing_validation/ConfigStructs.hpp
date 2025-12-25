#ifndef CONFIGSTRUCTS_HPP
#define CONFIGSTRUCTS_HPP

#include <string>
#include <map>
#include <set>
#include <vector>

struct Location
{
    std::string path;
    std::set<std::string> methods;
    std::string root;
    std::string index;
    std::vector<std::string> cgi_extensions;
    std::string upload_path;
    bool autoindex;
    std::pair<int, std::string> redirect;
    bool allow_get;
    bool allow_post;
    bool allow_delete;

    Location()
    {
        path = "";
        root = "";
        index = "";
        upload_path = "";
        // cgi_extensions is empty by default
        autoindex = false;
        redirect = std::make_pair(0, "");
        allow_get = true;
        allow_post = true;
        allow_delete = true;
    }
};

struct Server
{
    int listen;
    std::string host;
    std::string max_size;
    std::string server_name;
    std::string root;
    std::string index;
    std::map<int, std::string> error_pages;
    Location locations[10];
    int location_count;

    Server()
    {
        listen = 8080;
        host = "127.0.0.1";
        // Default to a reasonably large max body size (e.g. 50 MB)
        max_size = "50m";
        server_name = "localhost";
        root = "./www";
        index = "index.html";
        location_count = 0;

        // Default error pages
        error_pages[404] = "/errors/404.html";
        error_pages[500] = "/errors/500.html";
        error_pages[505] = "/errors/505.html";
    }
};

// Container for multiple server configurations
struct Servers
{
    std::vector<Server> servers;

    Servers() {}

    void addServer(const Server &server)
    {
        servers.push_back(server);
    }

    size_t count() const
    {
        return servers.size();
    }

    bool empty() const
    {
        return servers.empty();
    }
};

#endif
