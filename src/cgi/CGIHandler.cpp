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
    return _outBuffer;
}

void CGIHandler::sendToScript() {
    if (_inBuffer.empty()) {
        // 数据写完了，主动关闭写端，脚本才会收到 EOF 从而停止读取
        if (_pipeIn[1] != -1) {
            close(_pipeIn[1]);
            _pipeIn[1] = -1;
        }
        return;
    }

    // 尝试写入数据
    ssize_t bytes_sent = write(_pipeIn[1], _inBuffer.c_str(), _inBuffer.size());

    if (bytes_sent > 0) {
        // 移除已经发送的部分
        _inBuffer.erase(0, bytes_sent);
        _bytesWritten += bytes_sent;
        
        // 如果发完了，记得关掉管道，告诉脚本“没数据了”
        if (_inBuffer.empty()) {
            close(_pipeIn[1]);
            _pipeIn[1] = -1;
        }
    } else if (bytes_sent == -1) {
        // 如果 errno 是 EAGAIN，说明管道满了，等下次 POLLOUT 再写
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            _isExited = true; // 发生严重错误
        }
    }
}

void CGIHandler::receiveFromScript() {
    char buffer[8192]; // 8K 缓冲区
    ssize_t bytes_read = read(_pipeOut[0], buffer, sizeof(buffer));

    if (bytes_read > 0) {
        // 把读到的东西存进 _outBuffer
        _outBuffer.append(buffer, bytes_read);
    } else if (bytes_read == 0) {
        // 脚本关闭了输出端，说明执行完毕并输出了所有内容
        _isExited = true; 
        if (_pipeOut[0] != -1) {
            close(_pipeOut[0]);
            _pipeOut[0] = -1;
        }
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            _isExited = true;
        }
    }
}

int CGIHandler::getPid() const
{
    return _pid;
}

int CGIHandler::getReadFd() const
{
    return _pipeOut[0];
}

int CGIHandler::getWriteFd() const
{
    return _pipeIn[1];
}

bool CGIHandler::isTimeout() const
{
    if (_pid <= 0 || _isExited) return false;

    time_t currentTime = std::time(NULL);
    // 假设超时时间是 30 秒，你可以定义在配置文件或宏里
    if (currentTime - _startTime > 30) {
        return true;
    }
    return false;
}

bool CGIHandler::isFinished()
{
    if (_isExited) return true; // 已经处理过了，直接返回
    if (_pid <= 0) return false;

    int status;
    // WNOHANG 表示：如果子进程没结束，立即返回 0，不阻塞
    pid_t result = waitpid(_pid, &status, WNOHANG);

    if (result == _pid) {
        // 子进程已退出
        _isExited = true;
        return true;
    } else if (result == 0) {
        // 子进程还在跑
        return false;
    } else {
        // 发生错误（例如进程被意外杀掉）
        _isExited = true;
        return true;
    }
}