#include "parsing/ConfigParser.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

static std::string intToString(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <fstream>

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


int main() {
    ConfigParser parser("webserv.conf");
    Server server = parser.parseServer();

    int port = server.listen;
    std::string root = server.root.empty() ? "./www" : server.root;
    // Create a TCP socket using IPv4.
    // AF_INET       : Specifies IPv4 addresses.
    // SOCK_STREAM   : Specifies a connection-oriented, reliable, byte-stream socket (TCP).
    // 0             : Let the system choose the default protocol for TCP (IPPROTO_TCP).
    // This socket will be used on the server side to accept connections from clients.

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    listen(server_sock, 10);
    std::cout << "✅ Server listening on http://localhost:" << port << std::endl;

    while (true) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0)
            continue;
        serveClient(client_sock, root);
        close(client_sock);
    }
    close(server_sock);
    return 0;
}
