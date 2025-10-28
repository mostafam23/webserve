#include "ServerMain.hpp"
#include "ClientHandler.hpp"
#include "../signals/SignalHandler.hpp"
#include "../concurrency/ClientRegistry.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int startServer(const Server &server) {
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

        // Handle client synchronously (no threads allowed in project constraints)
        handleClient(client_sock, server.root, server.index);
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
