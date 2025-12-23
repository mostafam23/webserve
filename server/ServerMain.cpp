#include "ServerMain.hpp"
#include "../signals/SignalHandler.hpp"
#include "../concurrency/ClientRegistry.hpp"
#include "../http/HttpUtils.hpp"
#include "../utils/Utils.hpp"
#include "../logging/Logger.hpp"
#include "CgiHandler.hpp"
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
static bool parseIPv4(const std::string &s, in_addr *out)
{
    unsigned long a = 0, b = 0, c = 0, d = 0;
    const char *p = s.c_str();
    char *end = 0;
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
    unsigned long ip = (a << 24) | (b << 16) | (c << 8) | d; // a*256^3 + b*256^2 + c*256 + d same as this because shifting 24 means 2^24 =>(2^8)^3=>(256)^3
    // htonl Stands for Host TO Network Long.
    //  Convert 2130706433 (127.0.0.1) from host byte order to network byte order
    //  using htonl() to ensure correct endianness for network operations.
    out->s_addr = htonl((uint32_t)ip); // convert 2130706433 to hexa and then read it from the right,then convert it to decimal
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
static std::string sanitizePath(const std::string &raw)
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
    for (size_t i = 0; i <= p.size(); ++i)
    {
        if (i == p.size() || p[i] == '/')
        {
            if (!seg.empty())
            {
                if (seg == ".")
                {
                    // skip
                }
                // Important note: the function does not allow escaping above root because it only pops if parts is non-empty.
                // This is safe behavior for preventing directory traversal above the root path you intended.
                else if (seg == "..")
                {
                    if (!parts.empty())
                        parts.pop_back();
                }
                else
                {
                    parts.push_back(seg);
                }
                seg.clear();
            }
        }
        else
            seg += p[i];
    }
    std::string out = "/";
    for (size_t i = 0; i < parts.size(); ++i)
    {
        out += parts[i];
        if (i + 1 < parts.size())
            out += "/";
    }
    return out;
}

static std::string joinPaths(const std::string &a, const std::string &b)
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;
    if (a[a.size() - 1] == '/')
    {
        if (b[0] == '/')
            return a + b.substr(1);
        return a + b;
    }
    if (b[0] == '/')
        return a + b;
    return a + "/" + b;
}

// this return the longest matching path
static const Location *matchLocation(const Server &server, const std::string &path)
{
    const Location *best = 0;
    size_t bestLen = 0;
    for (int i = 0; i < server.location_count; ++i)
    {
        const Location &loc = server.locations[i];
        const std::string &lp = loc.path;
        if (lp.empty())
            continue;
        if (path.find(lp) == 0)
        {
            if (lp.size() > bestLen)
            {
                best = &loc;
                bestLen = lp.size();
            }
        }
    }
    return best;
}

static bool isMethodAllowed(const Location *loc, const std::string &method)
{
    if (!loc)
        return true; // no restriction
    if (loc->methods.empty())
        return true; // Example:location /images { root /var/www/img; }, No “allow” or “methods” defined → GET, POST, etc. are all allowed.
    // What does .find() return? If the method exists → it returns an iterator pointing to that element.
    // If the method does NOT exist → it returns end() iterator.
    return loc->methods.find(method) != loc->methods.end();
}

static std::string buildErrorWithCustom(const Server &server, int code, const std::string &message)
{
    std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
    if (it == server.error_pages.end())
        return buildErrorResponse(code, message);
    // Resolve path relative to server.root
    std::string ep = it->second; // it->second = the value (path, ex: "/errors/404.html")
    if (!ep.empty() && ep[0] != '/')
        ep = std::string("/") + ep; // Converts "errors/404.html" → "/errors/404.html"
    std::string full = joinPaths(server.root, ep);
    // std::ifstream = input file stream It is used to read from a file. So this line creates a file stream named f to read a file.
    /*
    Opening with std::ios::binary tells C++:
    “Read the file exactly as it is, do not modify any bytes.”
    So no newline conversion happens(kermel so times eza fatana as text so l os byaaml convert lal \r\n to \n aw l aaks so hayda bi2aser bl css whtml w ...).
    */
    std::ifstream f(full.c_str(), std::ios::binary);
    if (!f)
        return buildErrorResponse(code, message);
    // The line below reads the entire file content (opened as f) from start to end and stores it into a std::string called bod
    std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::ostringstream resp; // It is like std::cout but instead of printing to the terminal, it builds a string in memory.
    // You use << to add text into it. At the end, you call .str() to get the complete string.
    resp << "HTTP/1.1 " << code << " "
         << (code == 404 ? "Not Found" : (code == 405 ? "Method Not Allowed" : (code == 500 ? "Internal Server Error" : "Error")))
         << "\r\n";
    resp << "Content-Type: text/html\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Connection: close\r\n\r\n";
    resp << body;
    return resp.str();
}

int startServers(const Servers &servers)
{
    // checking if there is servers in the vector servers
    if (servers.empty())
    {
        std::cerr << "Error: No servers to start" << std::endl;
        return EXIT_FAILURE;
    }
    // Create and bind all server sockets
    std::vector<sockaddr_in> server_addrs;
    for (size_t i = 0; i < servers.count(); ++i)
    {
        const Server &server = servers.servers[i];
        /*This tells the socket which address family (network type) you want to use.
        AF_INET = IPv4 addresses
        SOCK_STREAM : This tells the socket which communication type it will use. SOCK_STREAM = TCP
        TCP means: connection-based, reliable (guarantees delivery and order),used for HTTP, HTTPS, FTP, SSH, etc.
        The system knows protocol must be TCP, so we pass 0.
        */
        int server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0)
        {
            perror("socket");
            // Close previously created sockets
            for (size_t j = 0; j < g_server_socks.size(); ++j)
            {
                close(g_server_socks[j]);
            }
            return EXIT_FAILURE;
        }

        int opt = 1;
        // setsocket modifies the setting of the server socket before binding it to a port
        // SO_REUSEADDR means : You can restart your server immediately without waiting for the port to free up.Why? Because the OS keeps the port in a "TIME_WAIT" state for 30–120 seconds.
        // SOL_SOCKET : “Apply this option at the socket level”
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server.listen);

        if (!server.host.empty())
        {
            if (!parseIPv4(server.host, &addr.sin_addr))
                addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(server_sock, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            close(server_sock);
            // Close previously created sockets
            for (size_t j = 0; j < g_server_socks.size(); ++j)
            {
                close(g_server_socks[j]);
            }
            return EXIT_FAILURE;
        }

        if (listen(server_sock, 128) < 0)
        {
            perror("listen");
            close(server_sock);
            // Close previously created sockets
            for (size_t j = 0; j < g_server_socks.size(); ++j)
            {
                close(g_server_socks[j]);
            }
            return EXIT_FAILURE;
        }

        setNonBlocking(server_sock);
        g_server_socks.push_back(server_sock);
        server_addrs.push_back(addr);

        std::cout << "✅ Server " << (i + 1) << " listening on http://"
                  << server.host << ":" << server.listen << std::endl;
    }

    std::cout << "Press Ctrl+C to stop the servers\n==============================\n";

    // Set backward compatibility variable to first server socket
    if (!g_server_socks.empty())
    {
        g_server_sock = g_server_socks[0];
    }

    // Per-client buffers and request counters
    std::map<int, std::string> recvBuf;
    std::map<int, int> reqCount;
    std::set<int> clients;

    while (!g_shutdown)
    {
        fd_set readfds; // fd_set is a data structure used by the select() system call to keep track of a group of file descriptors (FDs) that we want to monitor.
        // Think of it like a list/collection of sockets that you tell Linux to “watch”.
        // So fd_set = “a set of sockets to check”.
        FD_ZERO(&readfds); // FD_ZERO initializes/clears the fd_set.Because each time before calling select(), you must prepare a fresh set of sockets to watch.
        // If you don’t clear it, it may contain old data → undefined behavior.

        int maxfd = -1;

        // Add all server sockets to select set
        for (size_t i = 0; i < g_server_socks.size(); ++i)
        {
            if (g_server_socks[i] != -1)
            {
                FD_SET(g_server_socks[i], &readfds);
                if (g_server_socks[i] > maxfd)
                {
                    maxfd = g_server_socks[i];
                }
            }
        }

        // Add all client sockets to select set
        for (std::set<int>::const_iterator it = clients.begin(); it != clients.end(); ++it)
        {
            FD_SET(*it, &readfds);
            if (*it > maxfd)
            {
                maxfd = *it;
            }
        }

        if (maxfd == -1)
        {
            // No valid file descriptors
            break;
        }

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("select");
            break;
        }

        // Check for new connections on any server socket
        for (size_t i = 0; i < g_server_socks.size(); ++i)
        {
            // New connections
            // when a server socket is ready to read yaany a new client is ready to connect
            // if (FD_ISSET(3, &readfds)) { ... }  // is server socket ready? 3 yaany server-socket
            /*
            ✅ A new client is connecting
            ❗ Not when a client sends data
            ❗ Not when server wants to write
            ✅ Only when a new TCP connection request arrives
            */
            if (g_server_socks[i] != -1 && FD_ISSET(g_server_socks[i], &readfds))
            {
                sockaddr_in client_addr; // Creates a structure to store the connecting client’s IP and port
                socklen_t client_len = sizeof(client_addr);

                for (;;)
                {
                    // why inifinte loop? Because the server accepts clients as many as possible until no more pending connections remain.
                    int client_sock = accept(g_server_socks[i], (sockaddr *)&client_addr, &client_len);
                    if (client_sock < 0)
                    {
                        if (errno == EAGAIN) // No more clients waiting to connect now.
                            break;
                        if (errno == EINTR) // Interrupt signal occurred (example: Ctrl+C or OS signal).Try again → continue;
                            continue;
                        perror("accept");
                        break;
                    }

                    setNonBlocking(client_sock); // future recv() or send() do not block the server remains responsive(saree3 l estijaba)
                    clients.insert(client_sock);
                    recvBuf[client_sock] = std::string();
                    reqCount[client_sock] = 0;
                    addClient(client_sock, i + 1);
                }
            }
        }

        // Handle readable clients (same logic as original)
        std::vector<int> toClose;
        for (std::set<int>::const_iterator it = clients.begin(); it != clients.end(); ++it)
        {
            int fd = *it;
            // when there is a connection for a client but no request yet
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
                        // Process request (same logic as original)
                        std::string request = recvBuf[fd];
                        reqCount[fd]++;
                        std::istringstream iss(request);
                        std::string method, path, version;
                        iss >> method >> path >> version;

                        std::string queryString = "";
                        size_t qPos = path.find('?');
                        if (qPos != std::string::npos)
                        {
                            queryString = path.substr(qPos + 1);
                            path = path.substr(0, qPos);
                        }

                        if (method.empty() || path.empty() || version.empty())
                        {
                            std::string error = buildErrorResponse(400, "Invalid request");
                            send(fd, error.c_str(), error.size(), 0);
                            toClose.push_back(fd);
                            break;
                        }

                        std::map<std::string, std::string> headers = parseHeaders(request);

                        // Find which server handled this request based on port
                        Server *target_server = NULL;
                        for (size_t s = 0; s < servers.count(); ++s)
                        {
                            if (servers.servers[s].listen == ntohs(server_addrs[s].sin_port))
                            {
                                target_server = const_cast<Server *>(&servers.servers[s]);
                                break;
                            }
                        }
                        if (!target_server)
                        {
                            target_server = const_cast<Server *>(&servers.servers[0]);
                        }

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
                        if (headers.find("connection") != headers.end())
                        {
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
                        int server_num = getClientServer(fd);
                        rlog << "[REQUEST #" << reqCount[fd] << "] Client " << fd;
                        if (server_num > 0)
                        {
                            rlog << " (Server " << server_num << "): ";
                        }
                        else
                        {
                            rlog << ": ";
                        }
                        rlog << method << " " << path << " " << version
                             << (client_wants_keepalive ? " (keep-alive)" : " (close)");
                        Logger::request(rlog.str());
                        // Routing: match location, enforce methods, resolve root and path
                        const Location *loc = matchLocation(*target_server, path);

                        // 1. Handle Redirection
                        if (loc && loc->redirect.first != 0)
                        {
                            std::ostringstream resp;
                            resp << "HTTP/1.1 " << loc->redirect.first << " Found\r\n"
                                 << "Location: " << loc->redirect.second << "\r\n"
                                 << "Content-Length: 0\r\n"
                                 << "Connection: close\r\n\r\n";
                            std::string response = resp.str();
                            send(fd, response.c_str(), response.size(), 0);
                            toClose.push_back(fd);
                            break;
                        }

                        // 2. Check Max Body Size
                        if (headers.count("content-length"))
                        {
                            long cl = std::atol(headers["content-length"].c_str());
                            long max = parseSize(target_server->max_size);
                            // Only enforce the limit if max > 0. A value of 0 means "no limit".
                            if (max > 0 && cl > max)
                            {
                                std::string error = buildErrorWithCustom(*target_server, 413, "Payload Too Large");
                                send(fd, error.c_str(), error.size(), 0);
                                toClose.push_back(fd);
                                break;
                            }
                        }

                        // Handle Uploads
                        if (loc && !loc->upload_store.empty() && method == "POST")
                        {
                            std::string boundary = "";
                            if (headers.count("content-type"))
                            {
                                std::string ct = headers["content-type"];
                                size_t pos = ct.find("boundary=");
                                if (pos != std::string::npos)
                                    boundary = "--" + ct.substr(pos + 9);
                            }

                            if (boundary.empty())
                            {
                                std::string error = buildErrorWithCustom(*target_server, 400, "Bad Request: No boundary");
                                send(fd, error.c_str(), error.size(), 0);
                                toClose.push_back(fd);
                                break;
                            }

                            std::string body = "";
                            size_t bodyPos = request.find("\r\n\r\n");
                            if (bodyPos != std::string::npos)
                                body = request.substr(bodyPos + 4);

                            size_t filenamePos = body.find("filename=\"");
                            if (filenamePos == std::string::npos)
                            {
                                std::string error = buildErrorWithCustom(*target_server, 400, "Bad Request: No filename");
                                send(fd, error.c_str(), error.size(), 0);
                                toClose.push_back(fd);
                                break;
                            }
                            filenamePos += 10;
                            size_t filenameEnd = body.find("\"", filenamePos);
                            std::string filename = body.substr(filenamePos, filenameEnd - filenamePos);

                            size_t contentStart = body.find("\r\n\r\n", filenameEnd);
                            if (contentStart == std::string::npos)
                            {
                                std::string error = buildErrorWithCustom(*target_server, 400, "Bad Request: No content");
                                send(fd, error.c_str(), error.size(), 0);
                                toClose.push_back(fd);
                                break;
                            }
                            contentStart += 4;

                            size_t contentEnd = body.find(boundary, contentStart);
                            if (contentEnd == std::string::npos)
                            {
                                contentEnd = body.size();
                            }
                            if (contentEnd > 2 && body[contentEnd - 2] == '\r' && body[contentEnd - 1] == '\n')
                                contentEnd -= 2;

                            std::string fileContent = body.substr(contentStart, contentEnd - contentStart);
                            std::string uploadPath = loc->upload_store + "/" + filename;
                            std::cout << "Saving file to: " << uploadPath << std::endl;

                            std::ofstream outFile(uploadPath.c_str(), std::ios::binary);
                            if (!outFile)
                            {
                                std::string error = buildErrorWithCustom(*target_server, 500, "Internal Server Error: Could not save file");
                                send(fd, error.c_str(), error.size(), 0);
                                toClose.push_back(fd);
                                break;
                            }
                            outFile.write(fileContent.c_str(), fileContent.size());
                            outFile.close();

                            std::string successPage = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='3;url=/upload.html'><link rel='stylesheet' href='/assets/css/styles.css'></head><body class='container' style='display:flex;justify-content:center;align-items:center;height:100vh;text-align:center;'><div><h1 style='color:var(--success)'>File Uploaded Successfully</h1><p>Saved as " + filename + "</p><p>Redirecting...</p></div></body></html>";
                            std::ostringstream resp;
                            resp << "HTTP/1.1 201 Created\r\n"
                                 << "Content-Type: text/html\r\n"
                                 << "Content-Length: " << successPage.size() << "\r\n"
                                 << "Connection: close\r\n\r\n"
                                 << successPage;
                            std::string response = resp.str();
                            send(fd, response.c_str(), response.size(), 0);
                            toClose.push_back(fd);
                            break;
                        }

                        std::string effectiveRoot;
                        if (loc && !loc->root.empty())
                        {
                            effectiveRoot = loc->root;
                        }
                        else
                        {
                            effectiveRoot = target_server->root;
                        }
                        std::string safePath = sanitizePath(path);
                        if (!isMethodAllowed(loc, method))
                        {
                            std::string error = buildErrorWithCustom(*target_server, 405, "Method Not Allowed");
                            send(fd, error.c_str(), error.size(), 0);
                            client_wants_keepalive = false;
                            toClose.push_back(fd);
                            break;
                        }
                        std::string fullPath = effectiveRoot + safePath;

                        // 3. Handle Directory & Autoindex
                        if (isDirectory(fullPath))
                        {
                            if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/')
                                fullPath += "/";

                            std::string indexFile = (loc && !loc->index.empty()) ? loc->index : target_server->index;
                            std::string indexPath = fullPath + indexFile;

                            std::ifstream f(indexPath.c_str());
                            if (f.good())
                            {
                                fullPath = indexPath;
                                f.close();
                            }
                            else if (loc && loc->autoindex)
                            {
                                std::string listing = generateDirectoryListing(fullPath, path);
                                std::ostringstream resp;
                                resp << "HTTP/1.1 200 OK\r\n"
                                     << "Content-Type: text/html\r\n"
                                     << "Content-Length: " << listing.size() << "\r\n"
                                     << (client_wants_keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n")
                                     << "\r\n"
                                     << listing;
                                std::string response = resp.str();
                                send(fd, response.c_str(), response.size(), 0);
                                if (!client_wants_keepalive)
                                    toClose.push_back(fd);
                                recvBuf[fd].clear();
                                continue;
                            }
                            else
                            {
                                // Directory without index and without autoindex -> 403 Forbidden usually, or let it fail as 404
                                std::string error = buildErrorWithCustom(*target_server, 403, "Forbidden");
                                send(fd, error.c_str(), error.size(), 0);
                                if (!client_wants_keepalive)
                                    toClose.push_back(fd);
                                recvBuf[fd].clear();
                                continue;
                            }
                        };

                        // 4. Handle CGI
                        if (loc && !loc->cgi_extension.empty())
                        {
                            size_t extPos = fullPath.rfind(loc->cgi_extension);
                            if (extPos != std::string::npos && extPos == fullPath.size() - loc->cgi_extension.size())
                            {
                                std::string body = "";
                                size_t bodyPos = request.find("\r\n\r\n");
                                if (bodyPos != std::string::npos)
                                {
                                    body = request.substr(bodyPos + 4);
                                }

                                std::string cgiOutput = CgiHandler::executeCgi(fullPath, method, queryString, body, headers, *target_server, *loc);

                                std::string response;
                                if (cgiOutput.find("HTTP/") == 0)
                                {
                                    response = cgiOutput;
                                }
                                else
                                {
                                    response = "HTTP/1.1 200 OK\r\n";
                                    response += cgiOutput;
                                }

                                send(fd, response.c_str(), response.size(), 0);
                                if (!client_wants_keepalive)
                                    toClose.push_back(fd);
                                recvBuf[fd].clear();
                                continue;
                            }
                        }
                        std::string contentType = "text/html";
                        size_t dot = fullPath.find_last_of('.');
                        if (dot != std::string::npos)
                        {
                            std::string ext = fullPath.substr(dot);
                            if (ext == ".css")
                                contentType = "text/css";
                            else if (ext == ".js")
                                contentType = "application/javascript";
                            else if (ext == ".json")
                                contentType = "application/json";
                            else if (ext == ".png")
                                contentType = "image/png";
                            else if (ext == ".jpg" || ext == ".jpeg")
                                contentType = "image/jpeg";
                            else if (ext == ".gif")
                                contentType = "image/gif";
                            else if (ext == ".ico")
                                contentType = "image/x-icon";
                        }

                        std::string response;

                        if (method == "GET")
                        {
                            std::ifstream f(fullPath.c_str(), std::ios::binary);
                            if (f)
                            {
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
                            }
                            else
                            {
                                response = buildErrorWithCustom(*target_server, 404, "Not Found");
                                client_wants_keepalive = false;
                            }
                        }
                        else if (method == "POST")
                        {
                            // Simple POST handler that creates/updates the file
                            std::ofstream out(fullPath.c_str(), std::ios::binary);
                            if (out)
                            {
                                // Get the request body (simplified - in real case, parse Content-Length and read body properly)
                                size_t body_pos = request.find("\r\n\r\n") + 4;
                                if (body_pos < request.length())
                                {
                                    std::string body = request.substr(body_pos);
                                    out << body;
                                    response = "HTTP/1.1 201 Created\r\n";
                                    response += "Content-Type: application/json\r\n";
                                    response += "Content-Length: 0\r\n";
                                    response += client_wants_keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
                                    response += "\r\n";
                                }
                                else
                                {
                                    response = buildErrorWithCustom(*target_server, 400, "Bad Request");
                                    client_wants_keepalive = false;
                                }
                            }
                            else
                            {
                                response = buildErrorWithCustom(*target_server, 500, "Internal Server Error");
                                client_wants_keepalive = false;
                            }
                        }
                        else if (method == "DELETE")
                        {
                            if (std::remove(fullPath.c_str()) == 0)
                            {
                                response = "HTTP/1.1 200 OK\r\n";
                                response += "Content-Type: application/json\r\n";
                                response += "Content-Length: 0\r\n";
                                response += client_wants_keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
                                response += "\r\n";
                            }
                            else
                            {
                                response = buildErrorWithCustom(*target_server, 404, "Not Found");
                                client_wants_keepalive = false;
                            }
                        }
                        else
                        {
                            response = buildErrorWithCustom(*target_server, 501, "Not Implemented");
                        }
                        send(fd, response.c_str(), response.size(), 0);
                        if (!client_wants_keepalive)
                        {
                            toClose.push_back(fd);
                            break;
                        }

                        if (reqCount[fd] >= 10)
                        {
                            toClose.push_back(fd);
                            break;
                        }
                        recvBuf[fd].clear();
                    }
                }
                // n will be 0 if the client closed the terminal => close the connection
                else if (n == 0)
                {
                    toClose.push_back(fd);
                    break;
                }
                // n will be -1 if the client is not writing anything in the terminal after entering the for (;;)
                else
                {
                    if (errno == EAGAIN)
                        break;
                    if (errno == EINTR)
                        continue;
                    perror("recv");
                    toClose.push_back(fd);
                    break;
                }
            }
        }

        // Close marked clients
        for (size_t i = 0; i < toClose.size(); ++i)
        {
            int fd = toClose[i];
            clients.erase(fd);
            recvBuf.erase(fd);
            reqCount.erase(fd);
            removeClient(fd);
            close(fd);
        }
    }

    // Cleanup
    std::cout << "\n[SHUTDOWN] Closing server sockets..." << std::endl;
    for (size_t i = 0; i < g_server_socks.size(); ++i)
    {
        if (g_server_socks[i] != -1)
        {
            close(g_server_socks[i]);
        }
    }
    g_server_socks.clear();

    for (size_t i = 0; i < g_active_clients.size(); ++i)
    {
        close(g_active_clients[i]);
    }
    std::cout << "[SHUTDOWN] Closed " << g_active_clients.size()
              << " active connections" << std::endl;

    std::cout << "✅ All servers stopped gracefully" << std::endl;
    return EXIT_SUCCESS;
}
