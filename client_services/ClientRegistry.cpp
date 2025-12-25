#include "ClientRegistry.hpp"
#include "../logging/Logger.hpp"
#include <sstream>
#include <map>

std::vector<int> g_active_clients;
std::map<int, int> g_client_server_map; // client_fd -> server_num

void addClient(int client_sock, int server_num) {
    g_active_clients.push_back(client_sock);
    g_client_server_map[client_sock] = server_num;
    {
        std::ostringstream oss;
        oss << "Client connected (fd=" << client_sock
            << ") on Server " << server_num
            << ". Active clients: " << g_active_clients.size();
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
    
    int server_num = 0;
    std::map<int, int>::iterator it = g_client_server_map.find(client_sock);
    if (it != g_client_server_map.end()) {
        server_num = it->second;
        g_client_server_map.erase(it);
    }
    
    {
        std::ostringstream oss;
        oss << "Client disconnected (fd=" << client_sock;
        if (server_num > 0) {
            oss << ") from Server " << server_num;
        } else {
            oss << ")";
        }
        oss << ". Active clients: " << g_active_clients.size();
        Logger::info(oss.str());
    }
}

int getClientServer(int client_sock) {
    std::map<int, int>::iterator it = g_client_server_map.find(client_sock);
    if (it != g_client_server_map.end()) {
        return it->second;
    }
    return 0; // Not found
}
