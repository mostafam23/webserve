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
    bool debugFlag = true;

    if (argc == 2) {
        configFile = argv[1];
        useConfig = true;
    } else if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [config.conf]\n";
        return 1;
    } else {
        std::cout << "[INFO] No configuration file provided — using built-in defaults.\n";
    }

    if (useConfig) {
        if (configFile.size() < 6 || configFile.substr(configFile.size() - 5) != ".conf") {
            std::cerr << "[ERROR] Configuration file must end with .conf\n";
            std::cerr << "Usage: " << argv[0] << " [config.conf]\n";
            return 1;
        }
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
