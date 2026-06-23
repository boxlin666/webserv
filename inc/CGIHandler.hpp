#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>

#include "HttpRequest.hpp"
#include "Router.hpp"

#define MAX_CGI_RESPONSE_SIZE 1024*1000  // 104857600
#define CGI_TIMEOUT_SEC 10

class CGIHandler {
   public:
    enum CGIState {
        CGI_INIT,
        CGI_EXECUTING,
        CGI_FINISHED,
        CGI_ERROR
    };

   private:
    pid_t    _pid;
    CGIState _state;
    time_t   _last_activity_time;
    int      _waitpid_status;

    int _pipeIn[2];
    int _pipeOut[2];

    std::string                        _scriptPath;
    std::string                        _binPath;
    std::map<std::string, std::string> _envMap;
    char**                             _envp;

    std::string _inBuffer;
    std::string _outBuffer;

    CGIHandler(const CGIHandler& other);
    CGIHandler& operator=(const CGIHandler& other);

    void _prepareEnvMap(const HttpRequest& req);
    void _mapToEnvp();
    void _clearEnvp();

   public:
    CGIHandler();
    ~CGIHandler();

    bool init(const HttpRequest& req, const RouterCtx& ctx);
    bool execute();

    int sendToScript();
    int receiveFromScript();

    int getPid() const;
    int getWriteFd() const;
    int getReadFd() const;

    CGIState getState() const;
    void     updateTime();
    bool     isTimeout();
    bool     checkChildProcess();

    std::string getRawResponse() const;
    int         get_waitpid_status() const;
    void        _close_all_pipes();
    void        reset();
};

#endif
