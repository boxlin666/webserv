#include "CGIHandler.hpp"

CGIHandler::CGIHandler() : _envp(NULL) {}

CGIHandler::~CGIHandler() {_clearEnvp();}

bool CGIHandler::init(const HttpRequest& req, const RouterCtx& ctx)
{
    _scriptPath = ctx.full_path;

    if (ctx.loc && !ctx.loc->cgi_path.empty()) {
        _binPath = ctx.loc->cgi_path;
    } else {
        return false;
    }

    // 其实已经在RequestHandler 阶段被检查过了，一旦发现ko,直接报错，不会流入CGIHandler::init内部
    //  1. 物理检查
    /*struct stat s;
    if (stat(_scriptPath.c_str(), &s) != 0 || !S_ISREG(s.st_mode)) { return false; }
    if (access(_binPath.c_str(), X_OK) != 0) { return false; }*/

    // 2. 填充环境变量
    char* raw_path = std::getenv("PATH");

    if (raw_path != NULL) {
        _envMap["PATH"] = std::string(raw_path);
    } else {
        // 防御性编程：万一系统真的没设 PATH，给个安全的默认值
        _envMap["PATH"] = "/usr/bin:/bin:/usr/local/bin";
    }
    _envMap["REQUEST_METHOD"]    = req.get_method();
    _envMap["QUERY_STRING"]      = req.get_querystring();
    _envMap["SCRIPT_FILENAME"]   = _scriptPath;  // 脚本绝对路径
    _envMap["PATH_INFO"]         = req.get_path();
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
    updateTime();

    return true;
}

bool CGIHandler::execute(const HttpRequest& req)
{
    if (this->_binPath.empty() || this->_scriptPath.empty()) {
        std::cerr << "[CGI Error] Cannot execute: _binPath or _scriptPath is EMPTY!" << std::endl;
        return false;  // 或者抛出异常，或者让状态机切到 500 错误
    }

    if (pipe(_pipeIn) < 0 || pipe(_pipeOut) < 0) return false;

    _backup_pipeIn_write = _pipeIn[1];
    _backup_pipeOut_read = _pipeOut[0];

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

        // 🌟 使用 c_str() 配合 strdup 动态拷贝类成员路径，彻底消除 C++ 的类型转换警告
        args[0] = strdup(this->_binPath.c_str());     // 解释器路径，例如 "/usr/bin/python3"
        args[1] = strdup(this->_scriptPath.c_str());  // 脚本路径，例如 "./www/cgi-scripts/test.py"
        args[2] = NULL;                               // 严格以 NULL 结尾

        // 3. 拦截防御：万一传入的路径为空，或者 strdup 失败
        if (args[0] == NULL || args[1] == NULL) {
            std::cerr << "[CGI Child Error] Dynamic strdup failed." << std::endl;
            free(args[0]);
            free(args[1]);
            exit(1);
        }
        // 第一个参数用绝对路径，第三个参数用真实的系统环境
        execve(args[0], args, _envp);
        std::cerr << "========================================" << std::endl;
        std::cerr << "[CGI ERROR] Execve failed!" << std::endl;
        std::cerr << "错误原因 (Reason): " << strerror(errno) << std::endl;
        std::cerr << "错误代码 (Errno): " << errno << std::endl;
        std::cerr << "========================================" << std::endl;
        exit(1);
    } else {  // 父进程
        // 立即关闭父进程不需要的端
        /*close(_pipeIn[0]);
        _pipeIn[0] = -1;
        close(_pipeOut[1]);
        _pipeOut[1] = -1;*/
        close_unused_pipes(req);

        // 设置为非阻塞
        if (_pipeIn[1] != -1) fcntl(_pipeIn[1], F_SETFL, O_NONBLOCK);
        if (_pipeOut[0] != -1) fcntl(_pipeOut[0], F_SETFL, O_NONBLOCK);

        updateTime();
    }
    return true;
}

void CGIHandler::close_unused_pipes(const HttpRequest& req)
{
    close(_pipeIn[0]);
    _pipeIn[0] = -1;
    close(_pipeOut[1]);
    _pipeOut[1] = -1;

    if (req.get_method() == "GET" && req.get_body().length() == 0) {
        if (_pipeIn[1] != -1) {
            close(_pipeIn[1]);
            _pipeIn[1] = -1;
        }
    }
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
        for (int i = 0; _envp[i] != NULL; i++) {
            free(_envp[i]);
            _envp[i] = NULL;
        }
        delete[] _envp;
    }
}

std::string CGIHandler::getRawResponse()
{ return _outBuffer; }

void CGIHandler::sendToScript()
{
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
    updateTime();
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
            _isExited = true;  // 发生严重错误
        }
    }
}

void CGIHandler::receiveFromScript()
{
    char    buffer[8192];  // 8K 缓冲区
    ssize_t bytes_read = read(_pipeOut[0], buffer, sizeof(buffer));

    if (bytes_read > 0) {
        // 把读到的东西存进 _outBuffer
        _outBuffer.append(buffer, bytes_read);
        updateTime();
    } else if (bytes_read == 0) {
        // 脚本关闭了输出端，说明执行完毕并输出了所有内容
        _isExited = true;
        if (_pipeOut[0] != -1) {
            close(_pipeOut[0]);
            _pipeOut[0] = -1;
        }
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) { _isExited = true; }
    }
}

int CGIHandler::getPid() const
{ return _pid; }

int CGIHandler::getReadFd() const
{ return _pipeOut[0]; }

int CGIHandler::getWriteFd() const
{ return _pipeIn[1]; }

int CGIHandler::getBackUpReadFd() const
{ return _backup_pipeOut_read; }

int CGIHandler::getBackUpWriteFd() const
{ return _backup_pipeIn_write; }

void CGIHandler::updateTime()
{ _last_activity_time = std::time(NULL); }

bool CGIHandler::isTimeout() const
{
    if (_pid <= 0 || _isExited) return false;

    return (std::time(NULL) - _last_activity_time) > 300;
}

bool CGIHandler::isFinished()
{
    if (_isExited) return true;  // 已经处理过了，直接返回
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

void CGIHandler::close_all_pipes()
{
    // 🌟 原则：关闭前必须校验 FD 是否合法（>= 0），关闭后立刻重置为 -1

    // 1. 关闭输入管道（父写子读）
    if (this->_pipeIn[0] != -1) {
        close(this->_pipeIn[0]);
        this->_pipeIn[0] = -1;
    }
    if (this->_pipeIn[1] != -1) {
        close(this->_pipeIn[1]);
        this->_pipeIn[1] = -1;
    }

    // 2. 关闭输出管道（子写父读）
    if (this->_pipeOut[0] != -1) {
        close(this->_pipeOut[0]);
        this->_pipeOut[0] = -1;
    }
    if (this->_pipeOut[1] != -1) {
        close(this->_pipeOut[1]);
        this->_pipeOut[1] = -1;
    }
}

void CGIHandler::reset()
{
    // 1. 强力销毁动态分配的 C 风格环境变量指针数组，防止内存泄漏！
    this->_clearEnvp(); // 调用你原有的清理函数，确保 _envp 变回 NULL

    this->_envp = NULL;

    // 2. 彻底清空容器和字符串缓冲区
    this->_envMap.clear();
    this->_scriptPath.clear();
    this->_binPath.clear();
    this->_inBuffer.clear();
    this->_outBuffer.clear();

    // 3. 重置所有基础类型变量与标志位
    this->_pid = -1;
    this->_last_activity_time = 0;
    this->_isExited = false;
    this->_bytesWritten = 0;

    // 4. 重置核心读写工作管道（注意：这里绝对不要调 close()！）
    // 真正的 close 必须由大管家 Cluster 判定并执行后，这里只做“数字归零”
    this->_pipeIn[0] = -1;
    this->_pipeIn[1] = -1;
    this->_pipeOut[0] = -1;
    this->_pipeOut[1] = -1;

    // 5. 重置你的“幽灵备份”变量，迎接下一个全新的 HTTP 请求
    this->_backup_pipeIn_write = -1;
    this->_backup_pipeOut_read = -1;
}