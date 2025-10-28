#include "ClientRegistry.hpp"
#include "../logging/Logger.hpp"
#include <sstream>

std::vector<int> g_active_clients;

void addClient(int client_sock) {
    g_active_clients.push_back(client_sock);
    {
        std::ostringstream oss;
        oss << "Client connected (fd=" << client_sock
            << "). Active clients: " << g_active_clients.size();
        Logger::info(oss.str());
    }
}

void removeClient(int client_sock) {
    for (std::vector<int>::iterator it = g_active_clients.begin();
         it != g_active_clients.end(); ++it) {
        if (*it == client_sock) {
            g_active_clients.erase(it);
            break;
        }
    }
    {
        std::ostringstream oss;
        oss << "Client disconnected (fd=" << client_sock
            << "). Active clients: " << g_active_clients.size();
        Logger::info(oss.str());
    }
}
