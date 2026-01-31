#include "CgiHandler.hpp"
#include "../utils/Utils.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <csignal>
#include <ctime>

// Helper to convert map to envp
static char **createEnv(const std::map<std::string, std::string> &envMap)
{
    char **env = new char *[envMap.size() + 1];
    int i = 0;
    for (std::map<std::string, std::string>::const_iterator it = envMap.begin(); it != envMap.end(); ++it)
    {
        std::string s = it->first + "=" + it->second;
        env[i] = new char[s.size() + 1];
        ft_strcpy(env[i], s.c_str());
        i++;
    }
    env[i] = NULL;
    return env;
}

CgiSession CgiHandler::startCgi(const std::string &scriptPath, const std::string &method, const std::string &query, const std::string &body, const std::map<std::string, std::string> &headers, int clientFd)
{
    CgiSession session;
    session.clientFd = clientFd;
    session.pid = -1;
    session.pipeOut = -1;
    session.startTime = time(NULL);

    // Use a temporary file for the request body to avoid pipe deadlocks with large bodies
    char temp_path[256];
    int fdIn = ft_file_open_temp(temp_path);
    if (fdIn < 0)
    {
        std::cerr << "Error: ft_file_open_temp() failed" << std::endl;
        return session;
    }

    if (!body.empty())
    {
        const char *ptr = body.c_str();
        size_t remaining = body.size();
        ssize_t n;
        while (remaining > 0)
        {
            n = write(fdIn, ptr, remaining);
            if (n > 0)
            {
                ptr += n;
                remaining -= n;
            }
            else if (n == 0)
            {
                break;
            }
            else
            {
                break;
            }
        }
    }
    ft_file_close(fdIn);
    fdIn = open("/tmp/webserv_temp", O_RDONLY);
    if (fdIn < 0)
    {
        return session;
    }

    int pipe_out[2];
    if (pipe(pipe_out) < 0)
    {
        ft_file_close(fdIn);
        return session;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        ft_file_close(fdIn);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return session;
    }

    if (pid == 0)
    {
        // Child process
        dup2(fdIn, STDIN_FILENO);         // it reads from the temp file instead of stdin(keyboard);
        dup2(pipe_out[1], STDOUT_FILENO); // it writes to the pipe instead of stdout(screen);

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
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        {
            std::string key = it->first;
            std::string val = it->second;

            // Convert key to uppercase and replace - with _
            std::string envKey = "HTTP_";
            for (size_t i = 0; i < key.size(); ++i)
            {
                char c = key[i];
                if (c == '-')
                    c = '_';
                else
                    c = toupper(c);
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

        const char *argv[] = {interpreter.c_str(), scriptPath.c_str(), NULL};

        execve(argv[0], (char *const *)argv, envp);
        // If execve returns, it failed - write error to pipe
        const char *error_msg = "Status: 500 Internal Server Error\r\n\r\n";
        write(pipe_out[1], error_msg, 36);
        close(pipe_out[1]);
        close(pipe_out[0]);
        ft_file_close(fdIn);
        // Child process ends here - return ends the forked child process
        exit(1);
    }

    // Parent process
    ft_file_close(fdIn);
    close(pipe_out[1]); // Close write end

    session.pid = pid;
    session.pipeOut = pipe_out[0];

    // Set pipe to non-blocking
    int flags = fcntl(pipe_out[0], F_GETFL, 0);
    fcntl(pipe_out[0], F_SETFL, flags | O_NONBLOCK);

    return session;
}
