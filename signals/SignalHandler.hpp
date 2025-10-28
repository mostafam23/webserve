#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <csignal>

extern volatile sig_atomic_t g_shutdown;
extern int g_server_sock;

void signalHandler(int signum);

#endif // SIGNAL_HANDLER_HPP
