#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <csignal>
#include <vector>

extern volatile sig_atomic_t g_shutdown;
extern std::vector<int> g_server_socks;

void signalHandler(int signum);

#endif // SIGNAL_HANDLER_HPP
