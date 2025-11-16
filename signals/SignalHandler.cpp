#include "SignalHandler.hpp"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

volatile sig_atomic_t g_shutdown = 0;
std::vector<int> g_server_socks;
// Backward compatibility for single-server function
int g_server_sock = -1;

void signalHandler(int signum) {
    std::cout << "\n\nShutdown signal received (" << signum << ")..." << std::endl;
    g_shutdown = 1;
    
    // Close all server sockets
    for (size_t i = 0; i < g_server_socks.size(); ++i) {
        if (g_server_socks[i] != -1) {
            close(g_server_socks[i]);
            g_server_socks[i] = -1;
        }
    }
    
    // Backward compatibility
    if (g_server_sock != -1) {
        close(g_server_sock);
        g_server_sock = -1;
    }
}
