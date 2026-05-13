#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <sys/types.h>
#include <ctime>
#include <string>
#include <cstring>
#include <cstdlib>
#include <map>

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

public:
    CGIHandler();
    ~CGIHandler();

    bool init();
    bool execute();

    void _mapToEnvp();
    void _clearEnvp();
    void sendToScript();
    void receiveFromScript();

    int getReadFd() const;
    int getWriteFd() const;
    bool isTimeout() const;
    bool isFinished() const;

    std::string getRawResponse();

};

#endif