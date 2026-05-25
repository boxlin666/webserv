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

#define MAX_CGI_RESPONSE_SIZE 10485760
#define CGI_TIMEOUT_SEC 10 

class CGIHandler
{
public:
enum CGIState {
    CGI_INIT,       // 初始化完成，准备执行
    CGI_EXECUTING,  // fork 完成，子进程正在运行，正在进行 IO 推拉
    CGI_FINISHED,   // 子进程正常结束，数据读取完毕
    CGI_ERROR       // 发生错误（如超时、execve 失败、管道断裂）
};
private:
    pid_t       _pid;
    CGIState    _state;
    time_t      _last_activity_time;

    int         _pipeIn[2];
    int         _pipeOut[2];

    int _backup_pipeIn_write;  //_pipeIn[1]
    int _backup_pipeOut_read; // _pipeOut[0] 

    std::string _scriptPath;
    std::string _binPath;
    std::map<std::string, std::string> _envMap;
    char**      _envp;

    std::string _inBuffer;
    std::string _outBuffer;
    size_t      _bytesWritten;

    CGIHandler(const CGIHandler& other);
    CGIHandler& operator=(const CGIHandler& other);

    void _prepareEnvMap(const HttpRequest& req); 
    void _mapToEnvp();
    void _clearEnvp();

    void _close_unused_pipes(const HttpRequest& req);
public:
    CGIHandler();
    ~CGIHandler();

    bool init(const HttpRequest& req, const RouterCtx& ctx);
    bool execute(const HttpRequest& req);

    int sendToScript();
    int receiveFromScript();

    int getPid() const;
    int getWriteFd() const;
    int getReadFd() const;

    int getBackUpReadFd() const;
    int getBackUpWriteFd() const;

    CGIState    getState() const;
    void updateTime();
    bool isTimeout();
    bool checkChildProcess();

    std::string getRawResponse();
    void _close_all_pipes();
    void close_unused_pipes(const HttpRequest& req);

    void reset();
};

#endif