#include "ServerMain.hpp"
#include "ClientHandler.hpp"
#include "../signals/SignalHandler.hpp"
#include "../concurrency/ClientRegistry.hpp"
#include "../http/HttpUtils.hpp"
#include "../utils/PathUtils.hpp"
#include "../logging/Logger.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>
#include <set>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>

// Minimal IPv4 parser: accepts dotted-quad "A.B.C.D" and fills in_addr
static bool parseIPv4(const std::string& s, in_addr* out)
{
    unsigned long a = 0, b = 0, c = 0, d = 0;
    const char* p = s.c_str();
    char* end = 0;
    if (!p || !*p)
        return false;

    a = std::strtoul(p, &end, 10);
    if (end == p || *end != '.')
        return false;
    p = end + 1;
    b = std::strtoul(p, &end, 10);
    if (end == p || *end != '.')
        return false;
    p = end + 1;
    c = std::strtoul(p, &end, 10);
    if (end == p || *end != '.')
        return false;
    p = end + 1;
    d = std::strtoul(p, &end, 10);
    if (end == p || *end != '\0')
        return false;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return false;
    unsigned long ip = (a << 24) | (b << 16) | (c << 8) | d;//a*256^3 + b*256^2 + c*256 + d same as this because shifting 24 means 2^24 =>(2^8)^3=>(256)^3
    //htonl Stands for Host TO Network Long.
    // Convert 2130706433 (127.0.0.1) from host byte order to network byte order 
    // using htonl() to ensure correct endianness for network operations.
    out->s_addr = htonl((uint32_t)ip); //convert 2130706433 to hexa and then read it from the right,then convert it to decimal
    /* Host vs Network Byte Order
    Host byte order – how your CPU stores multi-byte integers in memory.
    Most PCs use little-endian → least significant byte first.
    Example: 0xC0A8010A in memory: 0A 01 A8 C0
    Network byte order – standardized big-endian → most significant byte first.
    Always used in networking functions like bind(), connect(), sendto().
    Example: 0xC0A8010A in memory: C0 A8 01 0A
    */
    return true;
}


/*fcntl is a system call used to manipulate file descriptors.yaany yet7akam so fcntl can control the socket and change its flags
int flags = fcntl(fd, F_GETFL, 0); this means to get all the flags of this fd
fcntl(fd, F_SETFL, flags | O_NONBLOCK); haydi yaany yzeed aal flag also the )_NONBLOCK
flags msln ) 0_RDONLY, ...
*/
static void setNonBlocking(int fd) 
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0)
        return;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ===== Routing helpers =====
static std::string sanitizePath(const std::string& raw)
{
    // ensure starts with '/'
    std::string p = raw.empty() || raw[0] != '/' ? std::string("/") + raw : raw;
    // split and resolve .. and .
    /*
    The vector allocates space once (for 16 items)
    No reallocation happens until you exceed 16 items
    */
    std::vector<std::string> parts; 
    parts.reserve(16);
    std::string seg;
    for (size_t i = 0; i <= p.size(); ++i) {
        if (i == p.size() || p[i] == '/') {
            if (!seg.empty()) {
                if (seg == ".") {
                    // skip
                }
                //Important note: the function does not allow escaping above root because it only pops if parts is non-empty. 
                //This is safe behavior for preventing directory traversal above the root path you intended.
                else if (seg == "..") {
                    if (!parts.empty()) parts.pop_back();
                } 
                else {
                    parts.push_back(seg);
                }
                seg.clear();
            }
        } else seg += p[i];
    }
    std::string out = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        out += parts[i];
        if (i + 1 < parts.size()) out += "/";
    }
    return out;
}

static std::string joinPaths(const std::string& a, const std::string& b)
{
    if (a.empty()) 
        return b;
    if (b.empty()) 
        return a;
    if (a[a.size()-1] == '/') {
        if (b[0] == '/') 
            return a + b.substr(1);
        return a + b;
    }
    if (b[0] == '/') 
        return a + b;
    return a + "/" + b;
}

//this return the longest matching path
static const Location* matchLocation(const Server& server, const std::string& path)
{
    const Location* best = 0;
    size_t bestLen = 0;
    for (int i = 0; i < server.location_count; ++i) 
    {
        const Location& loc = server.locations[i];
        const std::string& lp = loc.path;
        if (lp.empty()) 
            continue;
        if (path.find(lp) == 0) {
            if (lp.size() > bestLen) 
            {
                best = &loc; 
                bestLen = lp.size(); 
            }
        }
    }
    return best;
}

static bool isMethodAllowed(const Location* loc, const std::string& method)
{
    if (!loc) return true; // no restriction
    if (loc->methods.empty()) return true;//Example:location /images { root /var/www/img; }, No “allow” or “methods” defined → GET, POST, etc. are all allowed.
    //What does .find() return? If the method exists → it returns an iterator pointing to that element.
    //If the method does NOT exist → it returns end() iterator.
    return loc->methods.find(method) != loc->methods.end();
}

static std::string buildErrorWithCustom(const Server& server, int code, const std::string& message)
{
    std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
    if (it == server.error_pages.end())
        return buildErrorResponse(code, message);
    // Resolve path relative to server.root
    std::string ep = it->second;// it->second = the value (path, ex: "/errors/404.html")
    if (!ep.empty() && ep[0] != '/') 
        ep = std::string("/") + ep;// Converts "errors/404.html" → "/errors/404.html"
    std::string full = joinPaths(server.root, ep);
    //std::ifstream = input file stream It is used to read from a file. So this line creates a file stream named f to read a file.
    /*
    Opening with std::ios::binary tells C++:
    “Read the file exactly as it is, do not modify any bytes.”
    So no newline conversion happens(kermel so times eza fatana as text so l os byaaml convert lal \r\n to \n aw l aaks so hayda bi2aser bl css whtml w ...).
    */
    std::ifstream f(full.c_str(), std::ios::binary);
    if (!f) 
        return buildErrorResponse(code, message);
    //The line below reads the entire file content (opened as f) from start to end and stores it into a std::string called bod
    std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::ostringstream resp;//It is like std::cout but instead of printing to the terminal, it builds a string in memory.
    //You use << to add text into it. At the end, you call .str() to get the complete string.
    resp << "HTTP/1.1 " << code << " "
         << (code==404?"Not Found":(code==405?"Method Not Allowed":(code==500?"Internal Server Error":"Error")))
         << "\r\n";
    resp << "Content-Type: text/html\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Connection: close\r\n\r\n";
    resp << body;
    return resp.str();
}

int startServer(const Server &server) {
    /*This tells the socket which address family (network type) you want to use.
    AF_INET = IPv4 addresses
    SOCK_STREAM : This tells the socket which communication type it will use. SOCK_STREAM = TCP
    TCP means: connection-based, reliable (guarantees delivery and order),used for HTTP, HTTPS, FTP, SSH, etc.
    The system knows protocol must be TCP, so we pass 0.
    */
    g_server_sock = socket(AF_INET, SOCK_STREAM, 0); 
    if (g_server_sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    int opt = 1;
    //setsocket modifies the setting of the server socket before binding it to a port
    //SO_REUSEADDR means : You can restart your server immediately without waiting for the port to free up.Why? Because the OS keeps the port in a "TIME_WAIT" state for 30–120 seconds.
    //SOL_SOCKET : “Apply this option at the socket level”
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server.listen);
    if (!server.host.empty()) {
        if (!parseIPv4(server.host, &addr.sin_addr)) {
            // Fallback to any address if parsing fails
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    } 
    else {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    if (bind(g_server_sock, (sockaddr *)&addr, sizeof(addr)) < 0) 
    {
        perror("bind");
        close(g_server_sock);
        return EXIT_FAILURE;
    }
    if (listen(g_server_sock, 128) < 0) 
    {
        perror("listen");
        close(g_server_sock);
        return EXIT_FAILURE;
    }
    setNonBlocking(g_server_sock);
    std::cout << "✅ Server listening on http://" << server.host << ":" << server.listen
              << "\nPress Ctrl+C to stop the server\n==============================\n";

    // Per-client buffers and request counters
    std::map<int, std::string> recvBuf;
    std::map<int, int> reqCount;
    std::set<int> clients;

    while (!g_shutdown) 
    {
        fd_set readfds;//fd_set is a data structure used by the select() system call to keep track of a group of file descriptors (FDs) that we want to monitor.
        //Think of it like a list/collection of sockets that you tell Linux to “watch”.
        //So fd_set = “a set of sockets to check”.
        FD_ZERO(&readfds);// FD_ZERO initializes/clears the fd_set.Because each time before calling select(), you must prepare a fresh set of sockets to watch.
        //If you don’t clear it, it may contain old data → undefined behavior.
        int maxfd = g_server_sock;
        FD_SET(g_server_sock, &readfds);//FD_SET = write a socket number on the board
        for (std::set<int>::const_iterator it = clients.begin(); it != clients.end(); ++it) 
        {
            FD_SET(*it, &readfds);
            if (*it > maxfd) 
                maxfd = *it;
        }
        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);//maxfd+1 because we need to tell how many file descriptors it should check and plus because form 0 to nfds - 1
        if (ready < 0) 
        {
            //during selection if we press cntrl-C
            if (errno == EINTR)
            {
                continue;
            }
            perror("select");
            break;
        }
        // New connections
        // when a server socket is ready to read yaany a new client is ready to connect
        // if (FD_ISSET(3, &readfds)) { ... }  // is server socket ready? 3 yaany server-socket
        /*
        ✅ A new client is connecting
        ❗ Not when a client sends data
        ❗ Not when server wants to write
        ✅ Only when a new TCP connection request arrives
        */
        if (FD_ISSET(g_server_sock, &readfds))
        {
            sockaddr_in client_addr;//Creates a structure to store the connecting client’s IP and port
            socklen_t client_len = sizeof(client_addr);
            for (;;) 
            {
                //why inifinte loop? Because the server accepts clients as many as possible until no more pending connections remain.
                int client_sock = accept(g_server_sock, (sockaddr *)&client_addr, &client_len);
                if (client_sock < 0) 
                {
                    if (errno == EAGAIN) //No more clients waiting to connect now.
                    {
                        break;
                    }
                    if (errno == EINTR) //Interrupt signal occurred (example: Ctrl+C or OS signal).Try again → continue;
                    {
                        continue;
                    }
                    perror("accept");
                    break;
                }
                setNonBlocking(client_sock);//future recv() or send() do not block the server remains responsive(saree3 l estijaba)
                clients.insert(client_sock);
                recvBuf[client_sock] = std::string();
                reqCount[client_sock] = 0;
                addClient(client_sock);
            }
        }
        // Handle readable clients
        std::vector<int> toClose;
        for (std::set<int>::const_iterator it = clients.begin(); it != clients.end(); ++it) 
        {
            int fd = *it;
            //when there is a connection for a client but no request yet
            if (!FD_ISSET(fd, &readfds))
            {
                continue;
            }
            char buffer[8192];
            for (;;) 
            {
                ssize_t n = recv(fd, buffer, sizeof(buffer), 0); // 0 in the last argument to make the recv works normally wohtout options
                if (n > 0) 
                {
                    recvBuf[fd].append(buffer, n);
                    if (isRequestComplete(recvBuf[fd].c_str(), (int)recvBuf[fd].size())) 
                    {
                        // Parse request line
                        std::string request = recvBuf[fd];
                        reqCount[fd]++;
                        std::istringstream iss(request);
                        std::string method, path, version;
                        iss >> method >> path >> version;
                        if (method.empty() || path.empty() || version.empty()) {
                            std::string error = buildErrorResponse(400, "Invalid request");
                            send(fd, error.c_str(), error.size(), 0);
                            toClose.push_back(fd);
                            break;
                        }
                        std::map<std::string, std::string> headers = parseHeaders(request);
                        /*
                        HTTP Connection Header Summary
                        ------------------------------
                        HTTP/1.0:
                        - Default: connection closes after each response.
                        - Connection: close      -> Explicit close (same as default).
                        - Connection: keep-alive -> Try persistent connection (non-standard extension).
                        - Connection: blabla     -> Unknown token; connection closes.

                        HTTP/1.1:
                        - Default: connection stays open (persistent).
                        - Connection: keep-alive -> Redundant; connection stays open.
                        - Connection: close      -> Server must close after the response.
                        - Connection: blabla     -> ignore → connection stays open.
                        */
                        bool client_wants_keepalive = true;
                        if (headers.find("connection") != headers.end()) {
                            std::string conn = headers["connection"];
                            for (size_t i = 0; i < conn.size(); ++i) 
                                conn[i] = tolower(conn[i]);
                            if (conn == "close") 
                                client_wants_keepalive = false;
                        }
                        if (version == "HTTP/1.0" && headers["connection"] != "Keep-Alive")
                            client_wants_keepalive = false;
                        // Log request
                        std::ostringstream rlog;
                        rlog << "[REQUEST #" << reqCount[fd] << "] Client " << fd << ": "
                             << method << " " << path << " " << version
                             << (client_wants_keepalive ? " (keep-alive)" : " (close)");
                        Logger::request(rlog.str());
                        // Routing: match location, enforce methods, resolve root and path
                        const Location* loc = matchLocation(server, path);
                        std::string effectiveRoot;
                        if (loc && !loc->root.empty()) {
                            effectiveRoot = loc->root;
                        } else {
                            effectiveRoot = server.root;
                        }
                        std::string safePath = sanitizePath(path);
                        if (!isMethodAllowed(loc, method)) 
                        {
                            std::string error = buildErrorWithCustom(server, 405, "Method Not Allowed");
                            send(fd, error.c_str(), error.size(), 0);
                            client_wants_keepalive = false;
                            toClose.push_back(fd);
                            break;
                        }
                        // Build response (GET/HEAD minimal) with per-location root
                        std::string full_path = joinPaths(effectiveRoot, safePath);
                        if (isDirectory(full_path)) {
                            if (!full_path.empty() && full_path[full_path.size() - 1] != '/')
                                full_path += "/";
                            full_path += server.index;
                        }
                        std::string contentType = "text/html";
                        size_t dot = full_path.find_last_of('.');
                        if (dot != std::string::npos) {
                            std::string ext = full_path.substr(dot);
                            if (ext == ".css") contentType = "text/css";
                            else if (ext == ".js") contentType = "application/javascript";
                            else if (ext == ".json") contentType = "application/json";
                            else if (ext == ".png") contentType = "image/png";
                            else if (ext == ".jpg" || ext == ".jpeg") contentType = "image/jpeg";
                            else if (ext == ".gif") contentType = "image/gif";
                            else if (ext == ".ico") contentType = "image/x-icon";
                        }

                        std::string response;
                        if (method == "GET") {
                            std::ifstream f(full_path.c_str(), std::ios::binary);
                            if (f) {
                                std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                                response = "HTTP/1.1 200 OK\r\n";
                                response += "Content-Type: " + contentType + "\r\n";
                                response += "Content-Length: " + intToString((int)body.size()) + "\r\n";
                                if (client_wants_keepalive)
                                    response += "Connection: keep-alive\r\n";
                                else
                                    response += "Connection: close\r\n";
                                response += "\r\n";
                                response += body;
                            } else {
                                response = buildErrorWithCustom(server, 404, "Not Found");
                                client_wants_keepalive = false;
                            }
                        } else if (method == "HEAD") {
                            std::ifstream f(full_path.c_str(), std::ios::binary);
                            if (f) {
                                f.seekg(0, std::ios::end);
                                size_t size = f.tellg();
                                response = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType +
                                           "\r\nContent-Length: " + intToString((int)size) + "\r\n";
                                response += client_wants_keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
                                response += "\r\n";
                            } else {
                                response = buildErrorWithCustom(server, 404, "Not Found");
                                client_wants_keepalive = false;
                            }
                        } else if (method == "OPTIONS") {
                            response = "HTTP/1.1 200 OK\r\nAllow: GET, HEAD, POST, PUT, DELETE, OPTIONS\r\nContent-Length: 0\r\n";
                            response += client_wants_keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
                            response += "\r\n";
                        } else {
                            response = buildErrorResponse(501, "Not Implemented");
                            client_wants_keepalive = false;
                        }
                        // Send response (best-effort single send for now)
                        (void)send(fd, response.c_str(), response.size(), 0);

                        // Reset buffer for next request or close
                        recvBuf[fd].clear();
                        if (!client_wants_keepalive) {
                            toClose.push_back(fd);
                        }
                        break; // process next ready fd
                    }
                }
                //n will be 0 if the client closed the terminal => close the connection 
                else if (n == 0) 
                {
                    // client closed
                    toClose.push_back(fd);
                    break;
                }
                //n will be -1 if the client is not writing anything in the terminal after entering the for (;;) 
                else 
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) 
                        break;
                    toClose.push_back(fd);
                    break;
                }
            }
        }

        for (size_t i = 0; i < toClose.size(); ++i) {
            int fd = toClose[i];
            if (clients.erase(fd)) {
                close(fd);
                removeClient(fd);
                recvBuf.erase(fd);
                reqCount.erase(fd);
            }
        }
    }

    std::cout << "\n[SHUTDOWN] Closing server socket..." << std::endl;
    if (g_server_sock != -1)
        close(g_server_sock);

    for (size_t i = 0; i < g_active_clients.size(); ++i)
        close(g_active_clients[i]);
    std::cout << "[SHUTDOWN] Closed " << g_active_clients.size()
              << " active connections" << std::endl;

    std::cout << "✅ Server stopped gracefully" << std::endl;
    return EXIT_SUCCESS;
}
