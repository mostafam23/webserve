#include <string>
#include <map>
#include <sstream>
#include <iostream>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    bool valid;
    
    HttpRequest() : valid(false) {}
};

class HttpParser {
public:
    static HttpRequest parse(const std::string& raw_request) {
        HttpRequest req;
        
        if (raw_request.empty()) {
            return req;
        }
        
        std::istringstream stream(raw_request);
        std::string line;
        
        // Parse request line: GET /path HTTP/1.1
        if (!std::getline(stream, line)) {
            return req;
        }
        
        // Remove \r if present
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line = line.substr(0, line.size() - 1);
        }
        
        std::istringstream request_line(line);
        request_line >> req.method >> req.path >> req.version;
        
        // Validate request line
        if (req.method.empty() || req.path.empty()) {
            std::cerr << "[ERROR] Invalid request line: " << line << std::endl;
            return req;
        }
        
        // Parse headers
        while (std::getline(stream, line)) {
            // Remove \r
            if (!line.empty() && line[line.size() - 1] == '\r') {
                line = line.substr(0, line.size() - 1);
            }
            
            // Empty line marks end of headers
            if (line.empty()) {
                break;
            }
            
            // Parse header: Key: Value
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                
                // Trim leading/trailing spaces from value
                size_t start = value.find_first_not_of(" \t");
                size_t end = value.find_last_not_of(" \t");
                
                if (start != std::string::npos) {
                    value = value.substr(start, end - start + 1);
                }
                
                req.headers[key] = value;
            }
        }
        
        // Read body (if any)
        std::string remaining((std::istreambuf_iterator<char>(stream)),
                              std::istreambuf_iterator<char>());
        req.body = remaining;
        
        req.valid = true;
        return req;
    }
    
    static bool isValidMethod(const std::string& method) {
        return method == "GET" || method == "POST" || method == "DELETE" || 
               method == "PUT" || method == "HEAD" || method == "OPTIONS";
    }
    
    static std::string buildErrorResponse(int code, const std::string& message) {
        std::ostringstream response;
        std::string status_text;
        
        switch (code) {
            case 400: status_text = "Bad Request"; break;
            case 404: status_text = "Not Found"; break;
            case 405: status_text = "Method Not Allowed"; break;
            case 500: status_text = "Internal Server Error"; break;
            default: status_text = "Error"; break;
        }
        
        std::string body = "<!DOCTYPE html>\n<html>\n<head><title>" + 
                          std::to_string(code) + " " + status_text + 
                          "</title></head>\n<body>\n<h1>" + 
                          std::to_string(code) + " " + status_text + 
                          "</h1>\n<p>" + message + "</p>\n" +
                          "<hr><p><small>WebServer/1.0</small></p>\n</body>\n</html>";
        
        response << "HTTP/1.1 " << code << " " << status_text << "\r\n";
        response << "Content-Type: text/html\r\n";
        response << "Content-Length: " << body.size() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << body;
        
        return response.str();
    }
    
private:
    // C++98 compatible to_string
    static std::string to_string(int n) {
        std::ostringstream oss;
        oss << n;
        return oss.str();
    }
};

// Helper function to check if request is complete
bool isRequestComplete(const char* buffer, size_t length) {
    if (length < 4) return false;
    
    // Look for \r\n\r\n (end of headers)
    for (size_t i = 0; i <= length - 4; ++i) {
        if (buffer[i] == '\r' && buffer[i+1] == '\n' &&
            buffer[i+2] == '\r' && buffer[i+3] == '\n') {
            return true;
        }
    }
    return false;
}

/*
 * USAGE IN YOUR SERVER:
 * 
 * char buffer[4096];
 * ssize_t bytes = read(client_sock, buffer, sizeof(buffer) - 1);
 * 
 * if (bytes <= 0) {
 *     // Handle error/disconnect
 *     return;
 * }
 * 
 * buffer[bytes] = '\0';
 * 
 * // Check if complete request received
 * if (!isRequestComplete(buffer, bytes)) {
 *     std::string error = HttpParser::buildErrorResponse(400, "Incomplete request");
 *     send(client_sock, error.c_str(), error.size(), 0);
 *     return;
 * }
 * 
 * // Parse the request
 * HttpRequest req = HttpParser::parse(std::string(buffer, bytes));
 * 
 * if (!req.valid) {
 *     std::string error = HttpParser::buildErrorResponse(400, "Invalid HTTP request");
 *     send(client_sock, error.c_str(), error.size(), 0);
 *     return;
 * }
 * 
 * if (!HttpParser::isValidMethod(req.method)) {
 *     std::string error = HttpParser::buildErrorResponse(405, 
 *         "Method '" + req.method + "' is not supported");
 *     send(client_sock, error.c_str(), error.size(), 0);
 *     return;
 * }
 * 
 * // Now process the valid request
 * std::cout << "[REQUEST] " << req.method << " " << req.path << " " 
 *           << req.version << std::endl;
 * 
 * // Check headers
 * if (req.headers.find("Host") != req.headers.end()) {
 *     std::cout << "[HOST] " << req.headers["Host"] << std::endl;
 * }
 * 
 * // Serve the file based on req.path
 */