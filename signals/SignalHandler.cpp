#include "SignalHandler.hpp"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

volatile sig_atomic_t g_shutdown = 0;
int g_server_sock = -1;

void signalHandler(int signum) {
    std::cout << "\n\nShutdown signal received (" << signum << ")..." << std::endl;
    g_shutdown = 1;
    if (g_server_sock != -1) {
        shutdown(g_server_sock, SHUT_RDWR);
        close(g_server_sock);
        g_server_sock = -1;
    }
}
