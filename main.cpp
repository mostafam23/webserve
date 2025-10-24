#include "parsing_validation/ConfigParser.hpp"
#include "parsing_validation/ConfigValidator.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <pthread.h>
#include <vector>
#include <csignal>
#include <arpa/inet.h>

// =========================================================
// =============== GLOBAL STATE / SIGNAL FIX ===============
// =========================================================

volatile sig_atomic_t g_shutdown = 0;
int g_server_sock = -1; // ✅ global so signalHandler can close it

pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
std::vector<int> g_active_clients;

// =========================================================
// ===================== UTILITIES ==========================
// =========================================================

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

// =========================================================
// ================= SIGNAL HANDLER FIX =====================
// =========================================================

void signalHandler(int signum) {
    std::cout << "\n\nShutdown signal received (" << signum << ")..." << std::endl;
    g_shutdown = 1;

    // ✅ Close server socket to unblock accept()
    if (g_server_sock != -1) {
        shutdown(g_server_sock, SHUT_RDWR);
        close(g_server_sock);
        g_server_sock = -1;
    }
}

// =========================================================
// ================= CLIENT MANAGEMENT ======================
// =========================================================

void addClient(int client_sock) {
    pthread_mutex_lock(&g_clients_mutex);
    g_active_clients.push_back(client_sock);
    std::cout << "[INFO] Client connected (fd=" << client_sock
              << "). Active clients: " << g_active_clients.size() << std::endl;
    pthread_mutex_unlock(&g_clients_mutex);
}

void removeClient(int client_sock) {
    pthread_mutex_lock(&g_clients_mutex);
    for (std::vector<int>::iterator it = g_active_clients.begin();
         it != g_active_clients.end(); ++it) {
        if (*it == client_sock) {
            g_active_clients.erase(it);
            break;
        }
    }
    std::cout << "[INFO] Client disconnected (fd=" << client_sock
              << "). Active clients: " << g_active_clients.size() << std::endl;
    pthread_mutex_unlock(&g_clients_mutex);
}

// =========================================================
// ===================== STRUCTURES =========================
// =========================================================

struct ClientData {
    int socket;
    std::string root;
    std::string index;

    ClientData(int s, const std::string &r, const std::string &i)
        : socket(s), root(r), index(i) {}
};

// =========================================================
// ===================== HTTP HELPERS =======================
// =========================================================

bool isRequestComplete(const char *buffer, size_t length) {
    if (length < 4)
        return false;

    for (size_t i = 0; i <= length - 4; ++i)
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
            buffer[i + 2] == '\r' && buffer[i + 3] == '\n')
            return true;

    for (size_t i = 0; i <= length - 2; ++i)
        if (buffer[i] == '\n' && buffer[i + 1] == '\n')
            return true;

    return false;
}

std::string buildErrorResponse(int code, const std::string &message) {
    std::string status;
    switch (code) {
    case 400:
        status = "Bad Request";
        break;
    case 404:
        status = "Not Found";
        break;
    case 405:
        status = "Method Not Allowed";
        break;
    case 500:
        status = "Internal Server Error";
        break;
    case 501:
        status = "Not Implemented";
        break;
    default:
        status = "Error";
        break;
    }

    std::string body = "<!DOCTYPE html>\n<html>\n<head><title>" +
                       intToString(code) + " " + status +
                       "</title></head>\n<body>\n<h1>" +
                       intToString(code) + " " + status +
                       "</h1>\n<p>" + message +
                       "</p>\n<hr><p><small>WebServer/1.0</small></p>\n</body>\n</html>";

    std::ostringstream resp;
    resp << "HTTP/1.1 " << code << " " << status << "\r\n"
         << "Content-Type: text/html\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n\r\n"
         << body;
    return resp.str();
}

std::map<std::string, std::string> parseHeaders(const std::string &request) {
    std::map<std::string, std::string> headers;
    std::istringstream stream(request);
    std::string line;
    std::getline(stream, line); // skip first line
    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            break;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos)
                value = value.substr(start);
            for (size_t i = 0; i < key.size(); ++i)
                key[i] = tolower(key[i]);
            headers[key] = value;
        }
    }
    return headers;
}

// =========================================================
// ================= CLIENT THREAD ==========================
// =========================================================

void *handleClient(void *arg) {
    ClientData *data = static_cast<ClientData *>(arg);
    int client_sock = data->socket;
    std::string root = data->root;
    std::string index = data->index;

    pthread_detach(pthread_self());
    addClient(client_sock);

    bool keep_alive = true;
    int request_count = 0;
    const int max_requests = 100;

    struct timeval timeout = {30, 0};
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (keep_alive && request_count < max_requests && !g_shutdown) {
        char buffer[8192];
        ssize_t total_read = 0;
        bool request_complete = false;

        for (int attempts = 0; attempts < 10; ++attempts) {
            ssize_t bytes =
                read(client_sock, buffer + total_read, sizeof(buffer) - total_read - 1);
            if (bytes <= 0) {
                keep_alive = false;
                break;
            }
            total_read += bytes;
            buffer[total_read] = '\0';
            if (isRequestComplete(buffer, total_read)) {
                request_complete = true;
                break;
            }
            if (total_read >= (ssize_t)sizeof(buffer) - 100) {
                std::string error = buildErrorResponse(400, "Request too large");
                send(client_sock, error.c_str(), error.size(), 0);
                keep_alive = false;
                break;
            }
            usleep(10000);
        }

        if (!request_complete || !keep_alive)
            break;

        std::string request(buffer, total_read);
        request_count++;

        std::istringstream iss(request);
        std::string method, path, version;
        iss >> method >> path >> version;

        if (method.empty() || path.empty() || version.empty()) {
            std::string error = buildErrorResponse(400, "Invalid request");
            send(client_sock, error.c_str(), error.size(), 0);
            break;
        }

        std::map<std::string, std::string> headers = parseHeaders(request);
        bool client_wants_keepalive = true;

        if (headers.find("connection") != headers.end()) {
            std::string conn = headers["connection"];
            for (size_t i = 0; i < conn.size(); ++i)
                conn[i] = tolower(conn[i]);
            if (conn == "close")
                client_wants_keepalive = false;
        }

        if (version == "HTTP/1.0" && headers["connection"] != "Keep-Alive")
            client_wants_keepalive = false;

        std::cout << "[REQUEST #" << request_count << "] Client " << client_sock << ": "
                  << method << " " << path << " " << version
                  << (client_wants_keepalive ? " (keep-alive)" : " (close)") << std::endl;

        std::string full_path = root + path;
        if (isDirectory(full_path)) {
            if (!full_path.empty() && full_path[full_path.size() - 1] != '/')
                full_path += "/";
            full_path += index;
        }

        std::string contentType = "text/html";
        size_t dot = full_path.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = full_path.substr(dot);
            if (ext == ".css")
                contentType = "text/css";
            else if (ext == ".js")
                contentType = "application/javascript";
            else if (ext == ".json")
                contentType = "application/json";
            else if (ext == ".png")
                contentType = "image/png";
            else if (ext == ".jpg" || ext == ".jpeg")
                contentType = "image/jpeg";
            else if (ext == ".gif")
                contentType = "image/gif";
            else if (ext == ".ico")
                contentType = "image/x-icon";
        }

        std::string response;
        if (method == "GET") {
            std::ifstream file(full_path.c_str(), std::ios::binary);
            if (file) {
                std::string body((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: " + contentType + "\r\n";
                response += "Content-Length: " + intToString(body.size()) + "\r\n";
                if (client_wants_keepalive && request_count < max_requests) {
                    response += "Connection: keep-alive\r\n";
                    response += "Keep-Alive: timeout=30, max=" +
                                intToString(max_requests - request_count) + "\r\n";
                } else {
                    response += "Connection: close\r\n";
                    keep_alive = false;
                }
                response += "\r\n";
                response += body;
            } else {
                response = buildErrorResponse(404, "Not Found");
                keep_alive = false;
            }
        } else if (method == "HEAD") {
            std::ifstream file(full_path.c_str(), std::ios::binary);
            if (file) {
                file.seekg(0, std::ios::end);
                size_t size = file.tellg();
                response = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType +
                           "\r\nContent-Length: " + intToString(size) + "\r\n";
                if (client_wants_keepalive)
                    response += "Connection: keep-alive\r\n";
                else
                    response += "Connection: close\r\n";
                response += "\r\n";
            } else {
                response = buildErrorResponse(404, "Not Found");
                keep_alive = false;
            }
        } else if (method == "OPTIONS") {
            response = "HTTP/1.1 200 OK\r\nAllow: GET, HEAD, POST, PUT, DELETE, OPTIONS\r\n"
                       "Content-Length: 0\r\n";
            if (client_wants_keepalive)
                response += "Connection: keep-alive\r\n";
            else {
                response += "Connection: close\r\n";
                keep_alive = false;
            }
            response += "\r\n";
        } else {
            response = buildErrorResponse(501, "Not Implemented");
            keep_alive = false;
        }

        send(client_sock, response.c_str(), response.size(), 0);
        if (!client_wants_keepalive)
            keep_alive = false;
    }

    close(client_sock);
    removeClient(client_sock);
    delete data;
    return NULL;
}

// =========================================================
// ========================== MAIN ==========================
// =========================================================

int main(int argc, char *argv[]) {
    std::string configFile;
    bool useConfig = false;

    if (argc == 2) {
        configFile = argv[1];
        useConfig = true;
    } else {
        std::cout << "[INFO] No configuration file provided — using built-in defaults.\n";
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "=== WebServer Starting ===" << std::endl;
    if (useConfig)
        std::cout << "Config file: " << configFile << std::endl;

    Server server;

    if (useConfig) {
        try {
            std::cout << "Validating configuration file..." << std::endl;
            ConfigParser::validateConfigFile(configFile);
            ConfigParser parser(configFile);
            server = parser.parseServer();
            std::cout << "✓ Configuration parsed successfully!\n";
        } catch (const std::exception &e) {
            std::cerr << "[WARNING] Failed to load configuration: " << e.what()
                      << "\n[INFO] Continuing with defaults.\n";
        }
    }

    std::cout << "\nServer Configuration:\n";
    std::cout << "  Host:        " << server.host << "\n";
    std::cout << "  Port:        " << server.listen << "\n";
    std::cout << "  Server Name: " << server.server_name << "\n";
    std::cout << "  Root:        " << server.root << "\n";
    std::cout << "  Index:       " << server.index << "\n";
    std::cout << "  Max Size:    " << server.max_size << "\n\n";

    // Socket setup
    g_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server.host.c_str());
    addr.sin_port = htons(server.listen);

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

    std::cout << "✅ Server listening on http://" << server.host << ":" << server.listen
              << "\nPress Ctrl+C to stop the server\n==============================\n";

    while (!g_shutdown) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(g_server_sock, (sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (g_shutdown || errno == EBADF)
                break;
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        ClientData *data = new ClientData(client_sock, server.root, server.index);
        pthread_t thread;
        if (pthread_create(&thread, NULL, handleClient, data) != 0) {
            std::cerr << "[ERROR] Failed to create thread: " << strerror(errno) << std::endl;
            close(client_sock);
            delete data;
        }
    }

    std::cout << "\n[SHUTDOWN] Closing server socket..." << std::endl;
    if (g_server_sock != -1)
        close(g_server_sock);

    pthread_mutex_lock(&g_clients_mutex);
    for (size_t i = 0; i < g_active_clients.size(); ++i)
        close(g_active_clients[i]);
    std::cout << "[SHUTDOWN] Closed " << g_active_clients.size()
              << " active connections" << std::endl;
    pthread_mutex_unlock(&g_clients_mutex);
    pthread_mutex_destroy(&g_clients_mutex);

    std::cout << "✅ Server stopped gracefully" << std::endl;
    return EXIT_SUCCESS;
}
