#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <csignal>
#include <vector>

extern volatile sig_atomic_t g_shutdown;
extern std::vector<int> g_server_socks;
// Backward compatibility for single-server function
extern int g_server_sock;

void signalHandler(int signum);

#endif // SIGNAL_HANDLER_HPP
