#include "HttpUtils.hpp"
#include "../utils/Utils.hpp"
#include <sstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cctype>
#include <sstream>

std::string intToString(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

size_t getRequestLength(const char *buffer, size_t length) 
{
    if (length < 4)
    {
        return 0;
    }
    //for windows
    long headers_end = -1;
    for (size_t i = 0; i + 3 < length; ++i) {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
            buffer[i + 2] == '\r' && buffer[i + 3] == '\n') 
        {
            headers_end = (long)(i + 4);
            break;
        }
    }
    if (headers_end == -1)
    {
        return 0; // headers not complete yet
    }

    // Parse headers to determine body expectations
    // Optimization: Only construct string for headers to avoid copying large body
    std::string headerStr(buffer, headers_end);
    std::map<std::string, std::string> headers = parseHeaders(headerStr);

    // Check for Transfer-Encoding: chunked
    std::map<std::string, std::string>::iterator it = headers.find("transfer-encoding");
    if (it != headers.end()) 
    {
        std::string te = it->second;
        for (size_t i = 0; i < te.size(); ++i)
            te[i] = ft_tolower(te[i]);
        if (te.find("chunked") != std::string::npos) 
        {
            // Check if the zero chunk is right at the beginning of the body (empty body case)
            if (length >= (size_t)headers_end + 5 && ft_strncmp(buffer + headers_end, "0\r\n\r\n", 5) == 0)
            {
                return (size_t)headers_end + 5;
            }
            // Lenient check for LF only
            if (length >= (size_t)headers_end + 4 && ft_strncmp(buffer + headers_end, "0\n\n", 4) == 0)
            {
                return (size_t)headers_end + 4;
            }

            // For chunked, request complete when we see \r\n0\r\n\r\n after headers
            // We need to search in the body part
            std::string bodyPart(buffer + headers_end, length - headers_end);
            const std::string terminator = "\r\n0\r\n\r\n";
            size_t termPos = bodyPart.find(terminator);
            if (termPos != std::string::npos)
            {
                return headers_end + termPos + terminator.size();
            }
            // Some clients might send final chunk with LF only (rare) -> be lenient
            termPos = bodyPart.find("\n0\n\n");
            if (termPos != std::string::npos)
                return headers_end + termPos + 5; // \n0\n\n is 5 chars
            return 0;
        }
    }

    // Otherwise, rely on Content-Length if present, else no body
    it = headers.find("content-length");
    if (it != headers.end()) {
        long bodyLen = 0;
        std::istringstream iss(it->second);
        iss >> bodyLen;
        if (bodyLen < 0)
            bodyLen = 0;
        size_t total_needed = (size_t)headers_end + (size_t)bodyLen;
        if (length >= total_needed)
            return total_needed;
        return 0;
    }
    // No Content-Length and no chunked: assume no body (e.g., GET without body)
    return (size_t)headers_end;
}

std::string buildErrorResponse(int code, const std::string &message) 
{
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

std::map<std::string, std::string> parseHeaders(const std::string &request) {
    std::map<std::string, std::string> headers;
    std::istringstream stream(request);
    std::string line;
    ft_getline(stream, line); //This reads up to the first newline character (\n) in the stream  but does not include the '\n' in the resulting string.
    while (ft_getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            break;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = ft_substr(line, 0, colon);
            std::string value = ft_substr(line, colon + 1);
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos)
                value = ft_substr(value, start);
            for (size_t i = 0; i < key.size(); ++i)
                key[i] = ft_tolower(key[i]);
            headers[key] = value;
        }
    }
    return headers;
}

std::string unchunkBody(const std::string &body)
{
    std::string decoded;
    size_t i = 0;
    while (i < body.size())
    {
        size_t crlf = body.find("\r\n", i);
        if (crlf == std::string::npos)
            break;
        
        std::string hexLen = ft_substr(body, i, crlf - i);
        char *end;
        long len = ft_strtol(hexLen.c_str(), &end, 16);
        
        if (len == 0)
            break; // Last chunk

        i = crlf + 2;
        if (i + len > body.size())
            break; // Malformed or incomplete

        decoded.append(ft_substr(body, i, len));
        i += len + 2; // Skip data + CRLF
    }
    return decoded;
}
