*This project has been created as part of the 42 curriculum by mel-moha, ckinaan, and bmezher.*

## Description
WebServ is an HTTP/1.1 web server written in C++98. It uses non-blocking I/O (`select`) to handle multiple clients at once. It supports static websites, file uploads, and CGI scripts (Python, PHP). The goal is to understand how HTTP and socket programming works deep down.

## Instructions
**Compile:**
```bash
make
```

**Run:**
```bash
./webserv [config_file]
```
Example: `./webserv webserv.conf`

**Test:**
Open your browser and search `http://localhost:8080` (or the port in your config).

## Resources
**References:**
- [RFC 2616 (HTTP/1.1)](https://datatracker.ietf.org/doc/html/rfc2616)
- [Select() man page](https://man7.org/linux/man-pages/man2/select.2.html)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)

**AI Usage:**
We used AI (GitHub Copilot, ChatGPT) to:
- Explain complex socket functions and error handling.
- Debug segfaults and logic errors in the parser.
- Suggest improvements for code structure and the Makefile.
