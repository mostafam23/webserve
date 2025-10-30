#include "ServerMain.hpp"
#include "ClientHandler.hpp"
#include "../signals/SignalHandler.hpp"
#include "../concurrency/ClientRegistry.hpp"
#include "../http/HttpUtils.hpp"
#include "../utils/PathUtils.hpp"
#include "../logging/Logger.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>
#include <set>
#include <vector>
#include <sstream>
#include <fstream>

// Minimal IPv4 parser: accepts dotted-quad "A.B.C.D" and fills in_addr
static bool parseIPv4(const std::string& s, in_addr* out)
{
    unsigned long a = 0, b = 0, c = 0, d = 0;
    const char* p = s.c_str();
    char* end = 0;
    if (!p || !*p) return false;

    a = std::strtoul(p, &end, 10);
    if (end == p || *end != '.') return false;
    p = end + 1;
    b = std::strtoul(p, &end, 10);
    if (end == p || *end != '.') return false;
    p = end + 1;
    c = std::strtoul(p, &end, 10);
    if (end == p || *end != '.') return false;
    p = end + 1;
    d = std::strtoul(p, &end, 10);
    if (end == p || *end != '\0') return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    unsigned long ip = (a << 24) | (b << 16) | (c << 8) | d;//a*256^3 + b*256^2 + c*256 + d same as this because shifting 24 means 2^24 =>(2^8)^3=>(256)^3
    //htonl Stands for Host TO Network Long.
    out->s_addr = htonl((uint32_t)ip);//(uint32_t)ip Cast ip to an unsigned 32-bit integer.
    /* Host vs Network Byte Order
    Host byte order – how your CPU stores multi-byte integers in memory.
    Most PCs use little-endian → least significant byte first.
    Example: 0xC0A8010A in memory: 0A 01 A8 C0
    Network byte order – standardized big-endian → most significant byte first.
    Always used in networking functions like bind(), connect(), sendto().
    Example: 0xC0A8010A in memory: C0 A8 01 0A
    */
    return true;
static void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int startServer(const Server &server) {
    g_server_sock = socket(AF_INET, SOCK_STREAM, 0); 
    if (g_server_sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server.listen);
    std::cout << server.host << std::endl;
    if (!server.host.empty()) {
        if (!parseIPv4(server.host, &addr.sin_addr)) {
            // Fallback to any address if parsing fails
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    } else {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    std::cout << server.host << std::endl;
    if (bind(g_server_sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_server_sock);
        return EXIT_FAILURE;
    }

    if (listen(g_server_sock, 128) < 0) {
        perror("listen");
        close(g_server_sock);
        return EXIT_FAILURE;
    }

    setNonBlocking(g_server_sock);

    std::cout << "✅ Server listening on http://" << server.host << ":" << server.listen
              << "\nPress Ctrl+C to stop the server\n==============================\n";

    // Per-client buffers and request counters
    std::map<int, std::string> recvBuf;
    std::map<int, int> reqCount;
    std::set<int> clients;

    while (!g_shutdown) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = g_server_sock;
        FD_SET(g_server_sock, &readfds);
        for (std::set<int>::const_iterator it = clients.begin(); it != clients.end(); ++it) {
            FD_SET(*it, &readfds);
            if (*it > maxfd) maxfd = *it;
        }

        timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
        int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        // New connections
        if (FD_ISSET(g_server_sock, &readfds)) {
            sockaddr_in client_addr; socklen_t client_len = sizeof(client_addr);
            for (;;) {
                int client_sock = accept(g_server_sock, (sockaddr *)&client_addr, &client_len);
                if (client_sock < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    if (errno == EINTR) continue;
                    perror("accept");
                    break;
                }
                setNonBlocking(client_sock);
                clients.insert(client_sock);
                recvBuf[client_sock] = std::string();
                reqCount[client_sock] = 0;
                addClient(client_sock);
            }
        }

        // Handle readable clients
        std::vector<int> toClose;
        for (std::set<int>::const_iterator it = clients.begin(); it != clients.end(); ++it) {
            int fd = *it;
            if (!FD_ISSET(fd, &readfds)) continue;

            char buffer[8192];
            for (;;) {
                ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
                if (n > 0) {
                    recvBuf[fd].append(buffer, n);
                    if (isRequestComplete(recvBuf[fd].c_str(), (int)recvBuf[fd].size())) {
                        // Parse request line
                        std::string request = recvBuf[fd];
                        reqCount[fd]++;

                        std::istringstream iss(request);
                        std::string method, path, version;
                        iss >> method >> path >> version;
                        if (method.empty() || path.empty() || version.empty()) {
                            std::string error = buildErrorResponse(400, "Invalid request");
                            send(fd, error.c_str(), error.size(), 0);
                            toClose.push_back(fd);
                            break;
                        }

                        std::map<std::string, std::string> headers = parseHeaders(request);
                        bool client_wants_keepalive = true;
                        if (headers.find("connection") != headers.end()) {
                            std::string conn = headers["connection"];
                            for (size_t i = 0; i < conn.size(); ++i) conn[i] = tolower(conn[i]);
                            if (conn == "close") client_wants_keepalive = false;
                        }
                        if (version == "HTTP/1.0" && headers["connection"] != "Keep-Alive")
                            client_wants_keepalive = false;

                        // Log request
                        std::ostringstream rlog;
                        rlog << "[REQUEST #" << reqCount[fd] << "] Client " << fd << ": "
                             << method << " " << path << " " << version
                             << (client_wants_keepalive ? " (keep-alive)" : " (close)");
                        Logger::request(rlog.str());

                        // Build response (GET/HEAD minimal)
                        std::string full_path = server.root + path;
                        if (isDirectory(full_path)) {
                            if (!full_path.empty() && full_path[full_path.size() - 1] != '/')
                                full_path += "/";
                            full_path += server.index;
                        }
                        std::string contentType = "text/html";
                        size_t dot = full_path.find_last_of('.');
                        if (dot != std::string::npos) {
                            std::string ext = full_path.substr(dot);
                            if (ext == ".css") contentType = "text/css";
                            else if (ext == ".js") contentType = "application/javascript";
                            else if (ext == ".json") contentType = "application/json";
                            else if (ext == ".png") contentType = "image/png";
                            else if (ext == ".jpg" || ext == ".jpeg") contentType = "image/jpeg";
                            else if (ext == ".gif") contentType = "image/gif";
                            else if (ext == ".ico") contentType = "image/x-icon";
                        }

                        std::string response;
                        if (method == "GET") {
                            std::ifstream f(full_path.c_str(), std::ios::binary);
                            if (f) {
                                std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                                response = "HTTP/1.1 200 OK\r\n";
                                response += "Content-Type: " + contentType + "\r\n";
                                response += "Content-Length: " + intToString((int)body.size()) + "\r\n";
                                if (client_wants_keepalive)
                                    response += "Connection: keep-alive\r\n";
                                else
                                    response += "Connection: close\r\n";
                                response += "\r\n";
                                response += body;
                            } else {
                                response = buildErrorResponse(404, "Not Found");
                                client_wants_keepalive = false;
                            }
                        } else if (method == "HEAD") {
                            std::ifstream f(full_path.c_str(), std::ios::binary);
                            if (f) {
                                f.seekg(0, std::ios::end);
                                size_t size = f.tellg();
                                response = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType +
                                           "\r\nContent-Length: " + intToString((int)size) + "\r\n";
                                response += client_wants_keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
                                response += "\r\n";
                            } else {
                                response = buildErrorResponse(404, "Not Found");
                                client_wants_keepalive = false;
                            }
                        } else if (method == "OPTIONS") {
                            response = "HTTP/1.1 200 OK\r\nAllow: GET, HEAD, POST, PUT, DELETE, OPTIONS\r\nContent-Length: 0\r\n";
                            response += client_wants_keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
                            response += "\r\n";
                        } else {
                            response = buildErrorResponse(501, "Not Implemented");
                            client_wants_keepalive = false;
                        }

                        // Send response (best-effort single send for now)
                        (void)send(fd, response.c_str(), response.size(), 0);

                        // Reset buffer for next request or close
                        recvBuf[fd].clear();
                        if (!client_wants_keepalive) {
                            toClose.push_back(fd);
                        }
                        break; // process next ready fd
                    }
                } else if (n == 0) {
                    // client closed
                    toClose.push_back(fd);
                    break;
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    toClose.push_back(fd);
                    break;
                }
            }
        }

        for (size_t i = 0; i < toClose.size(); ++i) {
            int fd = toClose[i];
            if (clients.erase(fd)) {
                close(fd);
                removeClient(fd);
                recvBuf.erase(fd);
                reqCount.erase(fd);
            }
        }
    }

    std::cout << "\n[SHUTDOWN] Closing server socket..." << std::endl;
    if (g_server_sock != -1)
        close(g_server_sock);

    for (size_t i = 0; i < g_active_clients.size(); ++i)
        close(g_active_clients[i]);
    std::cout << "[SHUTDOWN] Closed " << g_active_clients.size()
              << " active connections" << std::endl;

    std::cout << "✅ Server stopped gracefully" << std::endl;
    return EXIT_SUCCESS;
}
