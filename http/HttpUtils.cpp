#include "HttpUtils.hpp"
#include <sstream>
#include <string>

std::string intToString(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

bool isRequestComplete(const char *buffer, size_t length) {
    if (length < 4)
        return false;

    // Find end of headers: CRLFCRLF or LFLF
    long headers_end = -1;
    for (size_t i = 0; i + 3 < length; ++i) {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
            buffer[i + 2] == '\r' && buffer[i + 3] == '\n') {
            headers_end = (long)(i + 4);
            break;
        }
    }
    if (headers_end == -1) {
        for (size_t i = 0; i + 1 < length; ++i) {
            if (buffer[i] == '\n' && buffer[i + 1] == '\n') {
                headers_end = (long)(i + 2);
                break;
            }
        }
    }
    if (headers_end == -1)
        return false; // headers not complete yet

    // Parse headers to determine body expectations
    std::string req(buffer, length);
    std::map<std::string, std::string> headers = parseHeaders(req);

    // Check for Transfer-Encoding: chunked
    std::map<std::string, std::string>::iterator it = headers.find("transfer-encoding");
    if (it != headers.end()) {
        std::string te = it->second;
        for (size_t i = 0; i < te.size(); ++i) te[i] = tolower(te[i]);
        if (te.find("chunked") != std::string::npos) {
            // For chunked, request complete when we see \r\n0\r\n\r\n after headers
            const std::string terminator = "\r\n0\r\n\r\n";
            if (req.find(terminator, (size_t)headers_end) != std::string::npos)
                return true;
            // Some clients might send final chunk with LF only (rare) -> be lenient
            if (req.find("\n0\n\n", (size_t)headers_end) != std::string::npos)
                return true;
            return false;
        }
    }

    // Otherwise, rely on Content-Length if present, else no body
    it = headers.find("content-length");
    if (it != headers.end()) {
        long bodyLen = 0;
        std::istringstream iss(it->second);
        iss >> bodyLen;
        if (bodyLen < 0) bodyLen = 0;
        size_t total_needed = (size_t)headers_end + (size_t)bodyLen;
        return length >= total_needed;
    }

    // No Content-Length and no chunked: assume no body (e.g., GET without body)
    return true;
}

std::string buildErrorResponse(int code, const std::string &message) {
    std::string status;
    switch (code) {
    case 400: status = "Bad Request"; break;
    case 404: status = "Not Found"; break;
    case 405: status = "Method Not Allowed"; break;
    case 500: status = "Internal Server Error"; break;
    case 501: status = "Not Implemented"; break;
    default:  status = "Error"; break;
    }

    std::string body = "<!DOCTYPE html>\n<html>\n<head><title>" +
                       intToString(code) + " " + status +
                       "</title></head>\n<body>\n<h1>" +
                       intToString(code) + " " + status +
                       "</h1>\n<p>" + message +
                       "</p>\n<hr><p><small>WebServer/1.0</small></p>\n</body>\n</html>";

    std::ostringstream resp;
    resp << "HTTP/1.1 " << code << " " << status << "\r\n"
         << "Content-Type: text/html\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n\r\n"
         << body;
    return resp.str();
}

#include <cctype>
#include <sstream>

std::map<std::string, std::string> parseHeaders(const std::string &request) {
    std::map<std::string, std::string> headers;
    std::istringstream stream(request);
    std::string line;
    std::getline(stream, line);
    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            break;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos)
                value = value.substr(start);
            for (size_t i = 0; i < key.size(); ++i)
                key[i] = tolower(key[i]);
            headers[key] = value;
        }
    }
    return headers;
}
