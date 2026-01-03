#include "parsing_validation/ConfigParser.hpp"
#include "parsing_validation/ConfigValidator.hpp"
#include "server/ServerMain.hpp"
#include "signals/SignalHandler.hpp"
#include "logging/Logger.hpp"
#include "app/App.hpp"
#include <iostream>
#include <csignal>

int main(int argc, char *argv[]) {
    // Facade: App acts as the application controller/facade orchestrating subsystems
    App app;
    return app.run(argc, argv);
}
