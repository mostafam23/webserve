#include "parsing_validation/ConfigParser.hpp"
#include "parsing_validation/ConfigValidator.hpp"
#include "server/ServerMain.hpp"
#include "signals/SignalHandler.hpp"
#include "logging/Logger.hpp"
#include <iostream>
#include <csignal>

// =========================================================
// ========================== MAIN ==========================
// =========================================================

int main(int argc, char *argv[]) {
    std::string configFile;
    bool useConfig = false;
    bool debugFlag = false;

    if (argc >= 2) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--debug") {
                debugFlag = true;
            } else if (configFile.empty()) {
                configFile = arg;
                useConfig = true;
            }
        }
    } else {
        std::cout << "[INFO] No configuration file provided — using built-in defaults.\n";
    }

    Logger::setDebugEnabled(debugFlag);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "=== WebServer Starting ===" << std::endl;
    if (Logger::isDebugEnabled())
        std::cout << "Debug mode: ON (colored logs enabled)\n";
    if (useConfig)
        std::cout << "Config file: " << configFile << std::endl;

    Server server;

    if (useConfig) {
        try {
            std::cout << "Validating configuration file..." << std::endl;
            ConfigParser::validateConfigFile(configFile);
            ConfigParser parser(configFile);
            server = parser.parseServer();
            std::cout << "✓ Configuration parsed successfully!\n";
        } catch (const std::exception &e) {
            std::cerr << "[WARNING] Failed to load configuration: " << e.what()
                      << "\n[INFO] Continuing with defaults.\n";
        }
    }

    std::cout << "\nServer Configuration:\n";
    std::cout << "  Host:        " << server.host << "\n";
    std::cout << "  Port:        " << server.listen << "\n";
    std::cout << "  Server Name: " << server.server_name << "\n";
    std::cout << "  Root:        " << server.root << "\n";
    std::cout << "  Index:       " << server.index << "\n";
    std::cout << "  Max Size:    " << server.max_size << "\n\n";

    return startServer(server);
}
