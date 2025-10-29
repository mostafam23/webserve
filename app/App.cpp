#include "App.hpp"

#include <iostream>
#include <vector>

#include "DebugPolicy.hpp"
#include "ArgValidation.hpp"
#include "ConfigSource.hpp"

#include "../logging/Logger.hpp"
#include "../signals/SignalHandler.hpp"
#include "../server/ServerMain.hpp"

static std::vector<std::string> collectArgs(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
    return args;
}

int App::run(int argc, char* argv[]) {
    // Strategy Pattern: choose how debug is determined.
    AlwaysOnDebugPolicy debugPolicy;
    Logger::setDebugEnabled(debugPolicy.isDebug());

    // Chain of Responsibility Pattern: validate CLI arguments in steps.
    ArgCountHandler count;
    ConfExtensionHandler confExt;
    count.setNext(&confExt);

    std::string error;
    std::vector<std::string> args = collectArgs(argc, argv);
    if (!count.process(args, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        std::cerr << "Usage: " << argv[0] << " [config.conf]\n";
        return 1;
    }

    // Null Object Pattern: pick default or file-based config source.
    IConfigSource* src = 0;
    DefaultConfigSource defaultSrc;
    FileConfigSource fileSrc( args.size() ? args[0] : std::string() );
    if (args.empty()) src = &defaultSrc; else src = &fileSrc;

    std::cout << "=== WebServer Starting ===" << std::endl;
    if (Logger::isDebugEnabled())
        std::cout << "Debug mode: ON (colored logs enabled)\n";
    if (!args.empty())
        std::cout << "Config file: " << args[0] << std::endl;

    // Signals
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Build server via config source and start
    Server server;
    try {
        server = src->buildServer();
        if (!args.empty())
            std::cout << "Configuration parsed successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "[WARNING] Failed to load configuration: " << e.what()
                  << "\n[INFO] Continuing with defaults.\n";
        server = Server();
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
