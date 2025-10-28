#include "ClientHandler.hpp"
#include "../signals/SignalHandler.hpp"
#include "../concurrency/ClientRegistry.hpp"
#include "../http/HttpUtils.hpp"
#include "../utils/PathUtils.hpp"
#include "../logging/Logger.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <map>
#include <netinet/in.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

void handleClient(int client_sock, const std::string &root, const std::string &index) {
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

        // Prepare client ip
        sockaddr_in peer; socklen_t plen = sizeof(peer);
        char ipbuf[64]; ipbuf[0] = '\0';
        if (getpeername(client_sock, (sockaddr*)&peer, &plen) == 0)
            std::strncpy(ipbuf, inet_ntoa(peer.sin_addr), sizeof(ipbuf)-1);
        else
            std::strncpy(ipbuf, "?", sizeof(ipbuf)-1);

        std::ostringstream rlog;
        rlog << "[REQUEST #" << request_count << "] Client " << client_sock << ": "
             << method << " " << path << " " << version
             << " " << ipbuf
             << (client_wants_keepalive ? " (keep-alive)" : " (close)");
        Logger::request(rlog.str());

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
                response += "Content-Length: " + intToString((int)body.size()) + "\r\n";
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
                           "\r\nContent-Length: " + intToString((int)size) + "\r\n";
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
}
