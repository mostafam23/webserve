#include "CgiHandler.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdio> // Added for tmpfile, fwrite, etc.

#include <iostream>
#include <sstream>
#include <cerrno>

// Helper to convert map to envp
static char **createEnv(const std::map<std::string, std::string> &envMap)
{
    char **env = new char *[envMap.size() + 1];
    int i = 0;
    for (std::map<std::string, std::string>::const_iterator it = envMap.begin(); it != envMap.end(); ++it)
    {
        std::string s = it->first + "=" + it->second;
        env[i] = new char[s.size() + 1];
        std::strcpy(env[i], s.c_str());
        i++;
    }
    env[i] = NULL;
    return env;
}

std::string CgiHandler::executeCgi(const std::string &scriptPath, const std::string &method, const std::string &query, const std::string &body, const std::map<std::string, std::string> &headers, const Server &server, const Location &loc)
{
    (void)server;
    (void)loc;
    
    // Use a temporary file for the request body to avoid pipe deadlocks with large bodies
    FILE* tmpIn = std::tmpfile();
    if (!tmpIn)
    {
        std::cerr << "Error: tmpfile() failed" << std::endl;
        return "Status: 500 Internal Server Error\r\n\r\n";
    }
    
    if (!body.empty()) {
        std::fwrite(body.c_str(), 1, body.size(), tmpIn);
    }
    std::rewind(tmpIn);
    int fdIn = fileno(tmpIn);

    int pipe_out[2];
    if (pipe(pipe_out) < 0)
    {
        std::fclose(tmpIn);
        return "Status: 500 Internal Server Error\r\n\r\n";
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        std::fclose(tmpIn);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return "Status: 500 Internal Server Error\r\n\r\n";
    }

    if (pid == 0)
    {
        // Child process
        dup2(fdIn, STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);

        close(pipe_out[0]);
        close(pipe_out[1]);
        // fclose(tmpIn); // Not strictly necessary as we exec, but good practice if we weren't

        // Prepare environment
        std::map<std::string, std::string> env;
        env["REQUEST_METHOD"] = method;
        env["QUERY_STRING"] = query;
        env["CONTENT_LENGTH"] = headers.count("content-length") ? headers.at("content-length") : "0";
        env["CONTENT_TYPE"] = headers.count("content-type") ? headers.at("content-type") : "";
        env["SCRIPT_FILENAME"] = scriptPath;
        env["REDIRECT_STATUS"] = "200"; // Needed for PHP-CGI
        env["SERVER_PROTOCOL"] = "HTTP/1.1";
        env["GATEWAY_INTERFACE"] = "CGI/1.1";
        env["PATH_INFO"] = scriptPath;
        env["REQUEST_URI"] = scriptPath; // Add REQUEST_URI
        env["SCRIPT_NAME"] = scriptPath; // Add SCRIPT_NAME

        // Pass HTTP headers as HTTP_ variables
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
            std::string key = it->first;
            std::string val = it->second;
            
            // Convert key to uppercase and replace - with _
            std::string envKey = "HTTP_";
            for (size_t i = 0; i < key.size(); ++i) {
                char c = key[i];
                if (c == '-') c = '_';
                else c = toupper(c);
                envKey += c;
            }
            env[envKey] = val;
        }

        char **envp = createEnv(env);

        // Determine interpreter
        std::string interpreter = "/usr/bin/python3"; // Default
        if (scriptPath.find(".php") != std::string::npos)
        {
            interpreter = "/usr/bin/php-cgi";
        }
        else if (scriptPath.find(".bla") != std::string::npos)
        {
            interpreter = "./cgi_tester";
        }

        const char *argv[] = {interpreter.c_str(), scriptPath.c_str(), NULL};

        execve(argv[0], (char *const *)argv, envp);
        exit(1);
    }
    else
    {
        // Parent process
        std::fclose(tmpIn); // This closes the temp file and deletes it
        close(pipe_out[1]);

        // Read output
        std::string output;
        char buffer[4096];
        ssize_t n;
        while ((n = read(pipe_out[0], buffer, sizeof(buffer))) > 0)
        {
            output.append(buffer, n);
        }
        close(pipe_out[0]);
        
        int status;
        waitpid(pid, &status, 0);

        return output;
    }
}
