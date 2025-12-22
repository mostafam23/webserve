#include "CgiHandler.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <iostream>
#include <sstream>

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
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
    {
        return "Status: 500 Internal Server Error\r\n\r\n";
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        return "Status: 500 Internal Server Error\r\n\r\n";
    }

    if (pid == 0)
    {
        // Child process
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);

        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);

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

        char **envp = createEnv(env);

        // Determine interpreter
        std::string interpreter = "/usr/bin/python3"; // Default
        if (scriptPath.find(".php") != std::string::npos)
        {
            interpreter = "/usr/bin/php-cgi";
        }

        const char *argv[] = {interpreter.c_str(), scriptPath.c_str(), NULL};

        execve(argv[0], (char *const *)argv, envp);
        exit(1);
    }
    else
    {
        // Parent process
        close(pipe_in[0]);
        close(pipe_out[1]);

        write(pipe_in[1], body.c_str(), body.size());
        close(pipe_in[1]);

        // Read output
        std::string output;
        char buffer[4096];
        ssize_t n;
        while ((n = read(pipe_out[0], buffer, sizeof(buffer))) > 0)
        {
            output.append(buffer, n);
        }
        close(pipe_out[0]);
        waitpid(pid, NULL, 0);

        return output;
    }
}
