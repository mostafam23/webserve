#include "ServerMain.hpp"
#include "../signals/SignalHandler.hpp"
#include "../client_services/ClientRegistry.hpp"
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>

// Globals for non-blocking I/O
static std::map<int, std::string> g_sendBuf;
static std::set<int> g_closing_clients;
static std::map<int, CgiSession> cgi_sessions; // pipe_out -> session

// Minimal IPv4 parser: accepts dotted-quad "A.B.C.D" and fills in_addr
static bool parseIPv4(const std::string &s, in_addr *out)
{
    unsigned long a = 0, b = 0, c = 0, d = 0;
    const char *p = s.c_str();
    char *end = 0;
    if (!p || !*p)
        return false;

    a = ft_strtoul(p, &end, 10);
    if (end == p || *end != '.')
        return false;
    p = end + 1;
    b = ft_strtoul(p, &end, 10);
    if (end == p || *end != '.')
        return false;
    p = end + 1;
    c = ft_strtoul(p, &end, 10);
    if (end == p || *end != '.')
        return false;
    p = end + 1;
    d = ft_strtoul(p, &end, 10);
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
            return a + ft_substr(b, 1);
        return a + b;
    }
    if (b[0] == '/')
        return a + b;
    return a + "/" + b;
}

static bool isMethodAllowed(const Location *loc, const std::string &method)
{
    if (!loc)
        return true; // no restriction
    if (loc->methods.empty())
        return false;
    // What does .find() return? If the method exists → it returns an iterator pointing to that element.
    // If the method does NOT exist → it returns end() iterator.
    return loc->methods.find(method) != loc->methods.end();
}

// this return the longest matching path
static const Location *matchLocation(const Server &server, const std::string &path, const std::string &method)
{
    const Location *suffixMatch = 0;
    const Location *prefixMatch = 0;
    size_t prefixLen = 0;

    for (int i = 0; i < server.location_count; ++i)
    {
        const Location &loc = server.locations[i];
        const std::string &lp = loc.path;
        if (lp.empty())
            continue;

        // Handle extension matching (e.g. *.bla)
        if (lp[0] == '*' && lp.size() > 1)
        {
            std::string ext = ft_substr(lp, 1); // remove *
            if (path.size() >= ext.size() &&
                path.compare(path.size() - ext.size(), ext.size(), ext) == 0)
            {
                suffixMatch = &loc;
            }
        }
        else if (path.find(lp) == 0)
        {
            if (lp.size() > prefixLen)
            {
                prefixMatch = &loc;
                prefixLen = lp.size();
            }
        }
    }

    // 1. If Suffix allows method, it wins.
    if (suffixMatch && isMethodAllowed(suffixMatch, method))
        return suffixMatch;

    // 2. If Prefix allows method, it wins (since Suffix didn't allow it).
    if (prefixMatch && isMethodAllowed(prefixMatch, method))
        return prefixMatch;

    // 3. If neither allows method, who wins?
    // Usually Suffix has higher priority in Nginx regex vs prefix.
    if (suffixMatch)
        return suffixMatch;
    if (prefixMatch)
        return prefixMatch;

    return 0;
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

static void sendAll(int fd, const std::string &data)
{
    if (!data.empty())
        g_sendBuf[fd].append(data);
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
            ft_perror("socket");
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
        ft_memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;            // this socket address uses the IPv4 address family
        addr.sin_port = htons(server.listen); // Convert port number to network byte order

        if (!parseIPv4(server.host, &addr.sin_addr))
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(server_sock, (sockaddr *)&addr, sizeof(addr)) < 0) // bind the socket to the specified IP and port
        {
            ft_perror("bind");
            close(server_sock);
            // Close previously created sockets
            for (size_t j = 0; j < g_server_socks.size(); ++j)
            {
                close(g_server_socks[j]);
            }
            return EXIT_FAILURE;
        }
        if (listen(server_sock, 4096) < 0)
        {
            ft_perror("listen");
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

    // Per-client buffers and request counters
    std::map<int, std::string> recvBuf;
    std::map<int, int> reqCount;
    std::set<int> clients;

    while (!g_shutdown)
    {
        fd_set readfds;
        fd_set writefds;

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

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
            int fd = *it;
            FD_SET(fd, &readfds);
            if (!g_sendBuf[fd].empty())
                FD_SET(fd, &writefds);

            if (fd > maxfd)
            {
                maxfd = fd;
            }
        }

        // Add CGI pipes to select set
        for (std::map<int, CgiSession>::iterator it = cgi_sessions.begin(); it != cgi_sessions.end(); ++it)
        {
            int fd = it->first;
            FD_SET(fd, &readfds);
            if (fd > maxfd)
                maxfd = fd;
        }

        if (maxfd == -1)
        {
            // No valid file descriptors
            break;
        }

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(maxfd + 1, &readfds, &writefds, NULL, &tv);
        if (ready < 0)
        {
            if (errno == EINTR) // cntrol + c
                continue;

            ft_perror("select");
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

                // Accept all pending connections
                int accepted = 0;
                while (true)
                {
                    // Throttle: If we are near the FD limit, stop accepting.
                    // Let clients wait in the backlog (safe) rather than accepting and closing (error).
                    if (clients.size() >= 800)
                    {
                        break;
                    }

                    int client_sock = accept(g_server_socks[i], (sockaddr *)&client_addr, &client_len);
                    if (client_sock < 0)
                    {
                        // No more connections pending or error
                        break;
                    }

                    if (client_sock >= FD_SETSIZE)
                    {
                        // Should rarely happen due to clients.size() check, but safety first
                        close(client_sock);
                    }
                    else
                    {
                        setNonBlocking(client_sock);
                        clients.insert(client_sock);
                        recvBuf[client_sock] = std::string();
                        reqCount[client_sock] = 0;
                        addClient(client_sock, i + 1);
                    }

                    // Limit acceptance to prevent starvation of other sockets
                    accepted++;
                    if (accepted > 500)
                        break;
                }
            }
        }

        // Handle CGI Output
        std::vector<int> cgiToClose;
        for (std::map<int, CgiSession>::iterator it = cgi_sessions.begin(); it != cgi_sessions.end(); ++it)
        {
            int pipeFd = it->first;
            CgiSession &session = it->second;

            if (FD_ISSET(pipeFd, &readfds))
            {
                char buffer[4096];
                ssize_t n = read(pipeFd, buffer, sizeof(buffer));
                if (n > 0)
                {
                    session.responseBuffer.append(buffer, n);
                }
                else
                {
                    // EOF or Error
                    int status;
                    waitpid(session.pid, &status, 0);

                    std::string response;
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                    {
                        response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 0\r\n\r\n";
                    }
                    else if (WIFSIGNALED(status))
                    {
                        response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 0\r\n\r\n";
                    }
                    else
                    {
                        std::string cgiOutput = session.responseBuffer;
                        if (cgiOutput.find("HTTP/") == 0)
                        {
                            response = cgiOutput;
                        }
                        else
                        {
                            response = "HTTP/1.1 200 OK\r\n";
                            bool hasContentLength = false;
                            std::string lowerCgi = ft_substr(cgiOutput, 0, 1024);
                            for (size_t i = 0; i < lowerCgi.size(); ++i)
                                lowerCgi[i] = ft_tolower(lowerCgi[i]);

                            if (lowerCgi.find("content-length:") != std::string::npos)
                            {
                                hasContentLength = true;
                            }

                            if (!hasContentLength)
                            {
                                size_t bodyPos = cgiOutput.find("\r\n\r\n");
                                size_t headerEndLen = 4;
                                if (bodyPos == std::string::npos)
                                {
                                    bodyPos = cgiOutput.find("\n\n");
                                    headerEndLen = 2;
                                }

                                size_t bodySize = 0;
                                if (bodyPos != std::string::npos)
                                {
                                    bodySize = cgiOutput.size() - (bodyPos + headerEndLen);
                                }
                                std::ostringstream oss;
                                oss << "Content-Length: " << bodySize << "\r\n";
                                response += oss.str();
                            }
                            response += cgiOutput;
                        }
                    }
                    sendAll(session.clientFd, response);
                    if (!session.keepAlive)
                        g_closing_clients.insert(session.clientFd);
                    cgiToClose.push_back(pipeFd);
                    continue;
                }
            }

            // Timeout
            if (time(NULL) - session.startTime >= 5)
            {
                kill(session.pid, SIGKILL);
                waitpid(session.pid, NULL, 0);
                std::string body = "<html><head><title>508 Loop Detected</title></head><body><h1>508 Loop Detected</h1><p>The CGI script took too long to execute.</p></body></html>";
                std::ostringstream ss;
                ss << "HTTP/1.1 508 Loop Detected\r\n"
                   << "Content-Type: text/html\r\n"
                   << "Content-Length: " << body.size() << "\r\n"
                   << "Connection: close\r\n\r\n"
                   << body;
                std::string response = ss.str();
                sendAll(session.clientFd, response);
                if (!session.keepAlive)
                    g_closing_clients.insert(session.clientFd);
                cgiToClose.push_back(pipeFd);
            }
        }

        for (size_t i = 0; i < cgiToClose.size(); ++i)
        {
            close(cgiToClose[i]);
            cgi_sessions.erase(cgiToClose[i]);
        }

        std::vector<int> toClose;
        for (std::set<int>::const_iterator it = clients.begin(); it != clients.end(); ++it)
        {
            int fd = *it;
            if (!FD_ISSET(fd, &readfds))
            {
                continue;
            }

            char buffer[65536];
            // Read only once per select event to avoid errno usage
            ssize_t n = recv(fd, buffer, sizeof(buffer), 0); // 0 in the last argument to make the recv works normally wohtout options
            if (n > 0)
            {
                recvBuf[fd].append(buffer, n);

                while (true)
                {
                    size_t reqLen = getRequestLength(recvBuf[fd].c_str(), recvBuf[fd].size());
                    if (reqLen == 0)
                        break;

                    std::string request = ft_substr(recvBuf[fd], 0, reqLen);
                    recvBuf[fd] = ft_substr(recvBuf[fd], reqLen);
                    reqCount[fd]++;
                    std::istringstream iss(request);
                    std::string method, path, version;
                    iss >> method >> path >> version;

                    std::string queryString = "";
                    size_t qPos = path.find('?');
                    if (qPos != std::string::npos)
                    {
                        queryString = ft_substr(path, qPos + 1);
                        path = ft_substr(path, 0, qPos);
                    }

                    if (method.empty() || path.empty() || version.empty())
                    {
                        std::string error = buildErrorResponse(400, "Invalid request");
                        sendAll(fd, error);
                        g_closing_clients.insert(fd);
                        break;
                    }

                    std::map<std::string, std::string> headers = parseHeaders(request);

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
                    bool client_wants_keepalive = true;
                    if (headers.find("connection") != headers.end())
                    {
                        std::string conn = headers["connection"];
                        for (size_t i = 0; i < conn.size(); ++i)
                            conn[i] = ft_tolower(conn[i]);
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
                    const Location *loc = matchLocation(*target_server, path, method);

                    // Check Max Body Size
                    if (headers.count("content-length"))
                    {
                        long cl = ft_atol(headers["content-length"].c_str());
                        long max = parseSize(target_server->max_size);
                        // Only enforce the limit if max > 0. A value of 0 means "no limit".
                        if (max > 0 && cl > max)
                        {
                            std::string error = buildErrorWithCustom(*target_server, 413, "Payload Too Large");
                            sendAll(fd, error);
                            g_closing_clients.insert(fd);
                            break;
                        }
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
                        sendAll(fd, error);
                        client_wants_keepalive = false;
                        g_closing_clients.insert(fd);
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
                        else if (method == "GET")
                        {
                            if (loc && loc->autoindex)
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
                                sendAll(fd, response);
                                if (!client_wants_keepalive)
                                    g_closing_clients.insert(fd);
                                continue;
                            }
                            else
                            {
                                // Directory without index and without autoindex -> 404
                                std::string error = buildErrorWithCustom(*target_server, 404, "Not Found");
                                sendAll(fd, error);
                                if (!client_wants_keepalive)
                                    g_closing_clients.insert(fd);
                                continue;
                            }
                        }
                    };

                    // 4. Handle CGI
                    if (loc && !loc->cgi_extensions.empty())
                    {
                        bool isCgi = false;
                        for (size_t i = 0; i < loc->cgi_extensions.size(); ++i)
                        {
                            const std::string &ext = loc->cgi_extensions[i];
                            size_t extPos = fullPath.rfind(ext);
                            if (extPos != std::string::npos && extPos == fullPath.size() - ext.size())
                            {
                                isCgi = true;
                                break;
                            }
                        }

                        if (isCgi)
                        {
                            // If it's a POST request and the file doesn't exist, we treat it as a file upload/creation
                            // instead of trying to execute a non-existent script.
                            // If it's a DELETE request, we want to delete the file, not execute it.
                            if ((method == "POST" && access(fullPath.c_str(), F_OK) == -1) || method == "DELETE")
                            {
                                // Loop will continue to generic POST handler below
                            }
                            else
                            {
                                if (access(fullPath.c_str(), F_OK) == -1)
                                {
                                    std::string error = buildErrorWithCustom(*target_server, 404, "Not Found");
                                    sendAll(fd, error);
                                    if (!client_wants_keepalive)
                                        g_closing_clients.insert(fd);
                                    continue;
                                }

                                std::string body = "";
                                size_t bodyPos = request.find("\r\n\r\n");
                                if (bodyPos != std::string::npos)
                                {
                                    body = ft_substr(request, bodyPos + 4);
                                }

                                // Handle Chunked Encoding for CGI
                                if (headers.count("transfer-encoding"))
                                {
                                    std::string te = headers["transfer-encoding"];
                                    for (size_t i = 0; i < te.size(); ++i)
                                        te[i] = ft_tolower(te[i]);
                                    if (te.find("chunked") != std::string::npos)
                                    {
                                        body = unchunkBody(body);
                                        std::ostringstream ss;
                                        ss << body.size();
                                        headers["content-length"] = ss.str();
                                        headers.erase("transfer-encoding");
                                    }
                                }

                                // Handle special headers test case (X-Secret-Header-For-Test)
                                // (Already done in executeCgi via headers map)

                                CgiSession session = CgiHandler::startCgi(fullPath, method, queryString, body, headers, fd);
                                if (session.pipeOut != -1)
                                {
                                    session.keepAlive = client_wants_keepalive;
                                    cgi_sessions[session.pipeOut] = session;
                                    continue;
                                }
                                else
                                {
                                    std::string error = buildErrorWithCustom(*target_server, 500, "Internal Server Error");
                                    sendAll(fd, error);
                                    if (!client_wants_keepalive)
                                        g_closing_clients.insert(fd);
                                    continue;
                                }
                            }
                        }
                    }
                    // 6. Handle DELETE
                    if (method == "DELETE")
                    {
                        if (ft_remove(fullPath.c_str()) == 0)
                        {
                            std::ostringstream ss;
                            ss << "HTTP/1.1 204 No Content\r\n";
                            ss << "Content-Length: 0\r\n";
                            ss << (client_wants_keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n");
                            ss << "\r\n";
                            sendAll(fd, ss.str());
                        }
                        else
                        {
                            if (access(fullPath.c_str(), F_OK) == -1)
                            {
                                std::string error = buildErrorWithCustom(*target_server, 404, "Not Found");
                                sendAll(fd, error);
                            }
                            else
                            {
                                std::string error = buildErrorWithCustom(*target_server, 403, "Forbidden");
                                sendAll(fd, error);
                            }
                        }
                        if (!client_wants_keepalive)
                            g_closing_clients.insert(fd);
                        // recvBuf[fd].clear();
                        continue;
                    }

                    std::string contentType = "text/html";
                    size_t dot = fullPath.find_last_of('.');
                    if (dot != std::string::npos)
                    {
                        std::string ext = ft_substr(fullPath, dot);
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
                        sendAll(fd, response);
                        if (!client_wants_keepalive)
                        {
                            g_closing_clients.insert(fd);
                            break;
                        }
                    }
                    else if (method == "POST")
                    {
                        // Simple POST handler that creates/updates the file

                        struct stat st;
                        if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                        {
                            // It's a directory. We can't write to it as a file.
                            std::string error = buildErrorWithCustom(*target_server, 405, "Method Not Allowed");
                            sendAll(fd, error);
                            client_wants_keepalive = false;
                            toClose.push_back(fd);
                            break;
                        }

                        std::ofstream out(fullPath.c_str(), std::ios::binary);
                        if (out)
                        {
                            // Get the request body (simplified - in real case, parse Content-Length and read body properly)
                            size_t body_pos = request.find("\r\n\r\n");
                            if (body_pos != std::string::npos)
                            {
                                std::string body = ft_substr(request, body_pos + 4);
                                if (headers.count("transfer-encoding"))
                                {
                                    std::string te = headers["transfer-encoding"];
                                    for (size_t i = 0; i < te.size(); ++i)
                                        te[i] = ft_tolower(te[i]);
                                    if (te.find("chunked") != std::string::npos)
                                    {
                                        body = unchunkBody(body);
                                    }
                                }
                                out << body;
                                response = "HTTP/1.1 200 OK\r\n";
                                response += "Content-Type: text/plain\r\n";
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
                            std::cerr << "Error: Failed to open file for writing (generic POST): " << fullPath << " (" << strerror(errno) << ")" << std::endl;
                            response = buildErrorWithCustom(*target_server, 500, "Internal Server Error");
                            client_wants_keepalive = false;
                        }
                        sendAll(fd, response);
                        if (!client_wants_keepalive)
                        {
                            g_closing_clients.insert(fd);
                            break;
                        }

                        if (reqCount[fd] >= 10)
                        {
                            g_closing_clients.insert(fd);
                            break;
                        }
                        // recvBuf[fd].clear(); // Handled by loop
                    }
                    else
                    {
                        std::string response = buildErrorWithCustom(*target_server, 501, "Not Implemented");
                        sendAll(fd, response);
                        if (!client_wants_keepalive)
                        {
                            g_closing_clients.insert(fd);
                            break;
                        }
                    }
                }
            }
            // n will be 0 if the client closed the connection
            else if (n == 0)
            {
                toClose.push_back(fd);
            }
            // n will be -1 (error or would block)
            else
            {
                toClose.push_back(fd);
            }
        }

        // Handle writable clients
        for (std::set<int>::const_iterator it = clients.begin(); it != clients.end(); ++it)
        {
            int fd = *it;
            if (FD_ISSET(fd, &writefds) && !g_sendBuf[fd].empty())
            {
                ssize_t sent = send(fd, g_sendBuf[fd].c_str(), g_sendBuf[fd].size(), MSG_DONTWAIT);
                if (sent > 0)
                {
                    g_sendBuf[fd] = g_sendBuf[fd].substr(sent);
                }
                else if (sent == 0)
                {
                    toClose.push_back(fd);
                }
                else if (sent == -1)
                {
                    toClose.push_back(fd);
                }
            }
            // Check if we can close now
            if (g_sendBuf[fd].empty() && g_closing_clients.count(fd))
            {
                toClose.push_back(fd);
                g_closing_clients.erase(fd);
            }
        }

        // Remove duplicates from toClose
        std::sort(toClose.begin(), toClose.end());
        toClose.erase(std::unique(toClose.begin(), toClose.end()), toClose.end());

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
            close(g_server_socks[i]);
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
