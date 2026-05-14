#include "CGIHandler.hpp"

CGIHandler::CGIHandler() {}

CGIHandler::~CGIHandler() {}

bool CGIHandler::init(const HttpRequest& req, const RouterCtx& ctx)
{
    _scriptPath = ctx.full_path;

    if (ctx.loc && !ctx.loc->cgi_path.empty()) {
        _binPath = ctx.loc->cgi_path;
    } else {
        return false;
    }

    // 1. 物理检查
    struct stat s;
    if (stat(_scriptPath.c_str(), &s) != 0 || !S_ISREG(s.st_mode)) { return false; }
    if (access(_binPath.c_str(), X_OK) != 0) { return false; }

    // 2. 填充环境变量
    _envMap["REQUEST_METHOD"]    = req.get_method();
    _envMap["QUERY_STRING"]      = req.get_querystring();
    _envMap["SCRIPT_FILENAME"]   = _scriptPath;  // 脚本绝对路径
    _envMap["GATEWAY_INTERFACE"] = "CGI/1.1";
    _envMap["SERVER_PROTOCOL"]   = "HTTP/1.1";

    // 数字转字符串
    std::stringstream ss;
    ss << req.get_content_length();
    _envMap["CONTENT_LENGTH"] = ss.str();

    // 3. 处理 Body
    if (req.get_method() == "POST") { _inBuffer = req.get_body(); }

    // 4. 转换并记录开始时间（用于超时处理）
    _mapToEnvp();
    _startTime = std::time(NULL);

    return true;
}

bool CGIHandler::execute()
{
    if (pipe(_pipeIn) < 0 || pipe(_pipeOut) < 0) return false;
    _pid = fork();
    if (_pid < 0) return false;
    if (_pid == 0) {
        // 重定向 STDIN: 脚本从 _pipeIn[0] 读数据
        dup2(_pipeIn[0], STDIN_FILENO);
        // 重定向 STDOUT: 脚本把结果写进 _pipeOut[1]
        dup2(_pipeOut[1], STDOUT_FILENO);

        close(_pipeIn[0]);
        close(_pipeIn[1]);
        close(_pipeOut[0]);
        close(_pipeOut[1]);

        char* args[3];
        args[0] = (char*)_binPath.c_str();
        args[1] = (char*)_scriptPath.c_str();
        args[2] = NULL;

        execve(_binPath.c_str(), args, _envp);
        std::cerr << "Execve failed!" << std::endl;
        exit(1);
    }
    else { // 父进程
        // 立即关闭父进程不需要的端
        close(_pipeIn[0]);  _pipeIn[0] = -1;
        close(_pipeOut[1]); _pipeOut[1] = -1;

        // 设置为非阻塞
        fcntl(_pipeIn[1], F_SETFL, O_NONBLOCK);
        fcntl(_pipeOut[0], F_SETFL, O_NONBLOCK);

        _startTime = std::time(NULL);
    }
    return true;
}

void CGIHandler::_mapToEnvp()
{
    _clearEnvp();
    _envp = new char*[_envMap.size() + 1];

    int i = 0;
    for (std::map<std::string, std::string>::iterator it = _envMap.begin(); it != _envMap.end();
         ++it) {
        std::string entry = it->first + "=" + it->second;
        _envp[i]          = strdup(entry.c_str());
        i++;
    }
    _envp[i] = NULL;
}
void CGIHandler::_clearEnvp()
{
    if (_envp) {
        for (int i = 0; _envp[i] != NULL; i++) { free(_envp[i]); }
        delete[] _envp;
        _envp = NULL;
    }
}

std::string CGIHandler::getRawResponse()
{
    std::string res;

    return res;
}
