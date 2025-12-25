#include "App.hpp"

#include <iostream>
#include <vector>
#include <unistd.h>

#include "ArgValidation.hpp"
#include "ConfigSource.hpp"

#include "../logging/Logger.hpp"
#include "../signals/SignalHandler.hpp"
#include "../server/ServerMain.hpp"

static std::vector<std::string> collectArgs(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) 
        args.push_back(argv[i]);
    return args;
}

int App::run(int argc, char* argv[]) {
    std::string error;
    std::vector<std::string> args = collectArgs(argc, argv);
    if (!validateArgs(args, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        std::cerr << "Usage: " << argv[0] << " [config.conf]\n";
        return 1;
    }
    // Null Object Pattern: pick default or file-based config source.
    // Since we enforce one argument, we always use FileConfigSource.
    FileConfigSource fileSrc(args[0]);
    IConfigSource* src = &fileSrc;

    std::cout << "=== WebServer Starting ===" << std::endl;
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        std::cout << "Current working directory: " << cwd << std::endl;
    }
    std::cout << "Config file: " << args[0] << std::endl;
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Build servers via config source and start
    Servers servers;
    try {
        servers = src->buildServers();
    } catch (const std::exception& e) {
        return 1;
    }

    std::cout << "Configuration parsed successfully!\n";

    std::cout << "\n=== Server Configurations ===\n";
    for (size_t i = 0; i < servers.count(); ++i) {
        const Server& server = servers.servers[i];
        std::cout << "Server " << (i + 1) << ":\n";
        std::cout << "  Host:        " << server.host << "\n";
        std::cout << "  Port:        " << server.listen << "\n";
        std::cout << "  Server Name: " << server.server_name << "\n";
        std::cout << "  Root:        " << server.root << "\n";
        std::cout << "  Index:       " << server.index << "\n";
        std::cout << "  Max Size:    " << server.max_size << "\n";
        if (i < servers.count() - 1) std::cout << "\n";
    }
    std::cout << "==============================\n\n";

    return startServers(servers);
}
