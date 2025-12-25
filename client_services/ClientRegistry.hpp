#ifndef CLIENT_REGISTRY_HPP
#define CLIENT_REGISTRY_HPP

#include <vector>

extern std::vector<int> g_active_clients;

void addClient(int client_sock, int server_num);
void removeClient(int client_sock);
int getClientServer(int client_sock);

#endif