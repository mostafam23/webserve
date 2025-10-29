// Facade Pattern: App provides a simple entry point to start the server.
// Template Method: App::run() defines the high-level startup sequence.
#ifndef APP_HPP
#define APP_HPP

#include <string>

class App {
public:
    int run(int argc, char* argv[]);
};

#endif
