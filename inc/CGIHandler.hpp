#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <sys/types.h>
#include <ctime>
#include <string>
#include <cstring>
#include <cstdlib>
#include <map>
#include <unistd.h>
#include <sys/wait.h>

#include "Router.hpp"
#include "HttpRequest.hpp"

class CGIHandler
{
private:
    pid_t       _pid;
    time_t      _startTime;
    bool        _isExited;
    std::map<std::string, std::string> _envMap;

    int         _pipeIn[2];
    int         _pipeOut[2];

    std::string _scriptPath;
    std::string _binPath;
    char**      _envp;

    std::string _inBuffer;
    std::string _outBuffer;
    size_t      _bytesWritten;

    void prepare_envmap(const HttpRequest& req, const RouterCtx& ctx);
    CGIHandler(const CGIHandler& other);
    CGIHandler& operator=(const CGIHandler& other);

public:
    CGIHandler();
    ~CGIHandler();

    bool init(const HttpRequest& req, const RouterCtx& ctx);
    bool execute(const HttpRequest& req);

    void _mapToEnvp();
    void _clearEnvp();
    void sendToScript();
    void receiveFromScript();

    int getPid() const;
    int getReadFd() const;
    int getWriteFd() const;
    bool isTimeout() const;
    bool isFinished();

    std::string getRawResponse();
    void close_all_pipes();
    void close_unused_pipes(const HttpRequest& req);
};

#endif