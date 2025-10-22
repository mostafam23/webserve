#include "parsing_validation/ConfigParser.hpp"
#include "parsing_validation/ConfigValidator.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>

static std::string intToString(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

bool isDirectory(const std::string &path) {
    struct stat s;

    if (stat(path.c_str(), &s) == 0)
        return S_ISDIR(s.st_mode);
    return false;
}

void serveClient(int client_sock, const std::string &root) {
    char buffer[4096];
    ssize_t bytes = read(client_sock, buffer, sizeof(buffer) - 1);
    if (bytes <= 0)
        return;

    buffer[bytes] = '\0';
    std::string request(buffer);

    // Parse request path
    std::string path = "/index.html";
    size_t pos1 = request.find(" ");
    size_t pos2 = request.find(" ", pos1 + 1);
    if (pos1 != std::string::npos && pos2 != std::string::npos)
        path = request.substr(pos1 + 1, pos2 - pos1 - 1);
        
    std::string full_path = root + path;

    // If path is a directory, append /index.html
    if (isDirectory(full_path)) {
        if (full_path[full_path.size() - 1] != '/')
            full_path += "/";
        full_path += "index.html";
    }

    std::ifstream file(full_path.c_str(), std::ios::binary);
    std::string response;
    if (file) {
        std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
                   intToString(body.size()) + "\r\n\r\n" + body;
    } else {
        std::string body = "<h1>404 Not Found</h1><p>Could not open: " + full_path + "</p>";
        response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: " +
                   intToString(body.size()) + "\r\n\r\n" + body;
    }
    send(client_sock, response.c_str(), response.size(), 0);
}

int main(int argc, char *argv[]) {
    // Default config file
    std::string configFile = "webserv.conf";
    bool debug = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            debug = true;
        } else if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            } else {
                std::cerr << "Error: --config requires a filename" << std::endl;
                return EXIT_FAILURE;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -c, --config <file>  Specify config file (default: webserv.conf)\n"
                      << "  -d, --debug          Enable debug output during validation\n"
                      << "  -h, --help           Show this help message\n";
            return EXIT_SUCCESS;
        }
    }

    std::cout << "=== WebServer Starting ===" << std::endl;
    std::cout << "Config file: " << configFile << std::endl;
    std::cout << std::endl;

    // CRITICAL STEP: Validate configuration file before proceeding
    // This will print detailed error messages and exit if validation fails
    std::cout << "Validating configuration file..." << std::endl;
    ConfigParser::validateConfigFile(configFile, debug);
    std::cout << "✓ Configuration file is valid!" << std::endl;
    std::cout << std::endl;

    // Parse the configuration (now safe because validation passed)
    std::cout << "Parsing configuration..." << std::endl;
    ConfigParser parser(configFile);
    Server server = parser.parseServer();
    std::cout << "✓ Configuration parsed successfully!" << std::endl;
    std::cout << std::endl;

    // Display configuration
    int port = server.listen;
    std::string root = server.root.empty() ? "./www" : server.root;
    
    std::cout << "Server Configuration:" << std::endl;
    std::cout << "  Port:        " << port << std::endl;
    std::cout << "  Host:        " << server.host << std::endl;
    std::cout << "  Server Name: " << server.server_name << std::endl;
    std::cout << "  Root:        " << root << std::endl;
    std::cout << "  Max Size:    " << server.max_size << std::endl;
    std::cout << "  Index:       " << server.index << std::endl;
    std::cout << std::endl;

    // Create a TCP socket using IPv4.
    // AF_INET       : Specifies IPv4 addresses.
    // SOCK_STREAM   : Specifies a connection-oriented, reliable, byte-stream socket (TCP).
    // 0             : Let the system choose the default protocol for TCP (IPPROTO_TCP).
    // This socket will be used on the server side to accept connections from clients.

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // Set SO_REUSEADDR to avoid "Address already in use" errors
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_sock);
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_sock);
        return EXIT_FAILURE;
    }

    if (listen(server_sock, 10) < 0) {
        perror("listen");
        close(server_sock);
        return EXIT_FAILURE;
    }

    std::cout << "✅ Server listening on http://localhost:" << port << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << std::endl;

    // Main server loop
    while (true) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }
        
        serveClient(client_sock, root);
        close(client_sock);
    }
    
    close(server_sock);
    return EXIT_SUCCESS;
}