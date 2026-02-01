# WebServ

*This project has been created as part of the 42 curriculum by mostafam23, ckinaan, and bmezher.*

## Description

**WebServ** is a fully-functional HTTP/1.1 web server implementation written in C++98, designed to handle multiple clients simultaneously using non-blocking I/O. This project demonstrates a deep understanding of the HTTP protocol, socket programming, and server-side architecture.

### Key Features

- **HTTP/1.1 Protocol Support** - Full compliance with RFC 2616 specifications
- **Non-blocking I/O** - Efficient multiplexing using `select()` for handling multiple connections
- **CGI Support** - Execute dynamic scripts (Python, PHP, etc.) via Common Gateway Interface
- **Configuration File** - Flexible server setup through custom configuration files
- **Multi-Server Support** - Run multiple virtual servers with different configurations
- **HTTP Methods** - Support for GET, POST, and DELETE methods
- **File Upload** - Handle file uploads with configurable size limits
- **Error Pages** - Custom error page support (404, 500, 505, 508)
- **Static File Serving** - Efficient serving of HTML, CSS, images, and other static content
- **Directory Listing** - Automatic index generation for directories
- **Request Parsing** - Robust parsing and validation of HTTP requests
- **Response Generation** - Proper HTTP response formatting with headers

### Goals

The primary objective of this project is to:
- Understand the inner workings of web servers and the HTTP protocol
- Implement network programming concepts using sockets and I/O multiplexing
- Handle concurrent connections without blocking or threading
- Parse and validate HTTP requests according to RFC specifications
- Generate compliant HTTP responses
- Implement CGI for dynamic content generation
- Manage server configuration and routing

## Instructions

### Prerequisites

- **Operating System**: Linux or macOS
- **Compiler**: `g++` or `clang++` with C++98 standard support
- **Make**: GNU Make utility
- **Optional**: Python 3.x and/or PHP for CGI scripts

### Compilation

To compile the project, navigate to the project root and run:

```bash
make
```

This will generate the `webserv` executable.

To clean object files:

```bash
make clean
```

To clean all generated files including the executable:

```bash
make fclean
```

To recompile everything:

```bash
make re
```

### Configuration

The server uses configuration files to define its behavior. Two example configuration files are provided:

- `webserv.conf` - Single server configuration
- `multi_server.conf` - Multiple virtual servers configuration

#### Configuration File Format

```conf
server {
    listen 8080;                          # Port to listen on
    server_name localhost;                # Server name
    root www;                             # Root directory for files
    index index.html;                     # Default index file
    client_max_body_size 10M;            # Maximum request body size
    
    error_page 404 /errors/404.html;     # Custom error pages
    error_page 500 /errors/500.html;
    
    location / {
        allow_methods GET POST;           # Allowed HTTP methods
        autoindex on;                     # Enable directory listing
    }
    
    location /uploads {
        allow_methods GET POST DELETE;
        upload_path uploads;              # Upload directory
    }
    
    location /cgi-bin {
        allow_methods GET POST;
        cgi_extension .py .php;           # CGI script extensions
        cgi_path /usr/bin/python3 /usr/bin/php;
    }
}
```

### Execution

To start the server with a configuration file:

```bash
./webserv [configuration_file]
```

Examples:

```bash
# Using default configuration
./webserv webserv.conf

# Using multi-server configuration
./webserv multi_server.conf
```

Once the server is running, you can access it via:
- Web browser: `http://localhost:8080`
- Command line: `curl http://localhost:8080`

### Testing

#### Browser Testing
1. Start the server: `./webserv webserv.conf`
2. Open your browser and navigate to `http://localhost:8080`
3. Test the services page at `http://localhost:8080/services.html`

#### Command Line Testing

**GET Request:**
```bash
curl http://localhost:8080/
curl http://localhost:8080/about.html
```

**POST Request (File Upload):**
```bash
curl -X POST -d "test content" http://localhost:8080/uploads/test.txt
```

**DELETE Request:**
```bash
curl -X DELETE http://localhost:8080/uploads/test.txt
```

**CGI Script Execution:**
```bash
curl http://localhost:8080/cgi-bin/script.py
```

### Project Structure

```
webserve/
├── main.cpp                    # Entry point
├── Makefile                    # Build configuration
├── README.md                   # This file
├── webserv.conf               # Single server config
├── multi_server.conf          # Multi-server config
├── app/                       # Application core
│   ├── App.cpp
│   ├── App.hpp
│   └── ConfigSource.hpp
├── server/                    # Server implementation
│   ├── ServerMain.cpp
│   ├── ServerMain.hpp
│   ├── CgiHandler.cpp
│   └── CgiHandler.hpp
├── http/                      # HTTP utilities
│   ├── HttpUtils.cpp
│   └── HttpUtils.hpp
├── parsing_validation/        # Config & request parsing
│   ├── ConfigParser.cpp
│   ├── ConfigParser.hpp
│   ├── ConfigValidator.cpp
│   ├── ConfigValidator.hpp
│   ├── ConfigParser_Utils.cpp
│   └── ConfigStructs.hpp
├── client_services/           # Client connection handling
│   ├── ClientRegistry.cpp
│   └── ClientRegistry.hpp
├── signals/                   # Signal handling
│   ├── SignalHandler.cpp
│   └── SignalHandler.hpp
├── logging/                   # Logging system
│   ├── Logger.cpp
│   └── Logger.hpp
├── utils/                     # Utility functions
│   ├── Utils.cpp
│   └── Utils.hpp
└── www/                       # Web content
    ├── index.html
    ├── about.html
    ├── services.html
    ├── assets/
    │   └── css/
    │       └── styles.css
    ├── cgi-bin/               # CGI scripts
    │   ├── script.py
    │   ├── test.py
    │   ├── test.php
    │   └── env_info.py
    ├── errors/                # Error pages
    │   ├── 404.html
    │   ├── 500.html
    │   ├── 505.html
    │   └── 508.html
    └── uploads/               # Upload directory
```

## Resources

### Technical Documentation

- **[RFC 2616](https://www.rfc-editor.org/rfc/rfc2616)** - HTTP/1.1 Protocol Specification
- **[RFC 3875](https://www.rfc-editor.org/rfc/rfc3875)** - The Common Gateway Interface (CGI) Version 1.1
- **[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)** - Comprehensive guide to socket programming
- **[HTTP Made Really Easy](https://www.jmarshall.com/easy/http/)** - Simple HTTP protocol explanation
- **[MDN Web Docs - HTTP](https://developer.mozilla.org/en-US/docs/Web/HTTP)** - HTTP protocol reference and guides

### Tutorials & Articles

- **[Building a Simple HTTP Server](https://medium.com/from-the-scratch/http-server-what-do-you-need-to-know-to-build-a-simple-http-server-from-scratch-d1ef8945e4fa)** - HTTP server basics
- **[Understanding select() and poll()](https://www.ulduzsoft.com/2014/01/select-poll-epoll-practical-difference-for-system-architects/)** - I/O multiplexing comparison
- **[HTTP Request & Response Structure](https://www.tutorialspoint.com/http/http_requests.htm)** - Understanding HTTP messages
- **[CGI Programming Tutorial](https://www.tutorialspoint.com/python/python_cgi_programming.htm)** - CGI implementation guide

### C++ References

- **[C++98 Standard](https://www.cplusplus.com/reference/)** - C++ language reference
- **[Socket Programming in C++](https://www.geeksforgeeks.org/socket-programming-cc/)** - Socket API documentation
- **[POSIX System Calls](https://man7.org/linux/man-pages/)** - Linux manual pages for system calls

### Tools Used for Testing

- **curl** - Command-line HTTP client for testing requests
- **Postman** - API testing tool
- **Browser Developer Tools** - Network tab for inspecting HTTP traffic
- **siege** - HTTP load testing tool
- **valgrind** - Memory leak detection

### AI Usage

AI assistance (GitHub Copilot / ChatGPT) was utilized for the following aspects of this project:

#### Code Implementation (20-30%)
- **Boilerplate generation**: Creating class structures and header guards
- **Code suggestions**: Auto-completion for standard C++ patterns and socket programming
- **Documentation**: Generating inline comments and function documentation
- **Debugging assistance**: Identifying logic errors and suggesting fixes

#### Design & Architecture (10-15%)
- **Design patterns**: Suggestions for organizing server architecture
- **Configuration parsing**: Initial approach to parsing configuration files
- **Error handling**: Patterns for robust error management

#### Web Frontend (60-70%)
- **HTML/CSS design**: Professional dark theme design system for the web interface
- **Responsive layout**: Grid systems and responsive breakpoints
- **JavaScript functionality**: Form handling and fetch API usage for the services page
- **UI/UX patterns**: Modern design trends and animations

#### Documentation (40-50%)
- **README structure**: Organizing project documentation
- **Code comments**: Explaining complex algorithms and data structures
- **Configuration examples**: Creating sample configuration files

#### Research & Learning (30-40%)
- **RFC interpretation**: Understanding HTTP/1.1 specifications
- **CGI implementation**: Researching CGI environment variables and execution
- **Socket programming**: Learning non-blocking I/O and select() usage
- **HTTP parsing**: Understanding request/response format and validation

**Important Note**: While AI provided suggestions and accelerated development, all core logic, architecture decisions, and implementation details were carefully reviewed, understood, and often significantly modified or rewritten to ensure code quality, correctness, and adherence to 42 School norms. The project demonstrates a thorough understanding of HTTP protocol, network programming, and C++ development practices.

## Features & Capabilities

### Supported HTTP Methods

| Method | Description | Implementation |
|--------|-------------|----------------|
| GET | Retrieve resources | ✅ Full support with directory listing |
| POST | Submit data / Upload files | ✅ With configurable size limits |
| DELETE | Remove resources | ✅ With safety checks |

### Configuration Options

- Multiple server blocks (virtual hosting)
- Custom port binding per server
- Server name configuration
- Root directory specification
- Default index files
- Client body size limits
- Custom error pages
- Location-based routing
- Method restrictions per location
- CGI script execution
- Directory auto-indexing
- Upload directory specification

### Error Handling

The server provides comprehensive error handling:
- **400 Bad Request** - Malformed requests
- **404 Not Found** - Resource not found
- **405 Method Not Allowed** - Unsupported method for location
- **413 Payload Too Large** - Request body exceeds limit
- **500 Internal Server Error** - Server-side errors
- **505 HTTP Version Not Supported** - Non-HTTP/1.1 requests
- **508 Loop Detected** - Redirect loop prevention

### Security Features

- Request body size limits to prevent DoS
- Path traversal protection (../ prevention)
- Method restriction per location
- Safe file upload handling
- Timeout handling for connections
- Signal handling (SIGINT, SIGTERM) for graceful shutdown

## Authors

- **Mostafa El Mohammad** ([mostafam23](https://github.com/mostafam23)) - Architect
- **Carla Kinaan** ([ckinaan](https://github.com/CarlaKinaan)) - Lead Developer  
- **Bahaa Mezher** ([bmezher](https://github.com/bahaamezher3)) - Core Engineer

## License

This project is created as part of the 42 School curriculum and follows the school's academic policies.

## Acknowledgments

- **42 School** for providing the project specifications and learning environment
- **The HTTP/1.1 RFC Authors** for comprehensive protocol documentation
- **The open-source community** for excellent resources and tutorials on network programming

---

*For more information about 42 School and its projects, visit [42.fr](https://www.42.fr)*
