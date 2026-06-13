#include "CGIHandler.hpp"
#include "Utils.hpp"
#include "NotificationPipe.hpp"

// 1. 构造函数：必须严谨地初始化所有内置类型和状态
CGIHandler::CGIHandler()
    : _pid(-1), _state(CGI_INIT), _last_activity_time(0), _envp(NULL)
{
    _pipeIn[0]  = -1;
    _pipeIn[1]  = -1;
    _pipeOut[0] = -1;
    _pipeOut[1] = -1;
}

// 2. 析构函数：保证内存与文件描述符双重释放
CGIHandler::~CGIHandler()
{
    _clearEnvp();
    _close_all_pipes();
}

// 3. 剥离出来的环境准备函数（保持单一职责）
void CGIHandler::_prepareEnvMap(const HttpRequest& req)
{
    char* raw_path  = std::getenv("PATH");
    _envMap["PATH"] = (raw_path != NULL) ? std::string(raw_path) : "/usr/bin:/bin:/usr/local/bin";

    _envMap["REQUEST_METHOD"]    = req.get_method();
    _envMap["QUERY_STRING"]      = req.get_querystring();
    _envMap["SCRIPT_FILENAME"]   = _scriptPath;
    _envMap["PATH_INFO"]         = req.get_path();
    _envMap["GATEWAY_INTERFACE"] = "CGI/1.1";
    _envMap["SERVER_PROTOCOL"]   = "HTTP/1.1";
    _envMap["SERVER_SOFTWARE"]   = "Webserv/1.0";
  
    //需要通过判断是不是Chunked来决定添加content length 否则脚本可能会卡住！
    if (req.get_is_chunked() == false)
    {
        std::string req_content_length = Utils::toString(req.get_content_length());
        _envMap["CONTENT_LENGTH"] = req_content_length;
    }

    std::string req_content_type = req.get_header("Content-Type");

    if (req_content_type.empty()) 
        _envMap["CONTENT_TYPE"] = ""; 
    else
        _envMap["CONTENT_TYPE"] = req_content_type;
  
    std::string env_key;
    std::map<std::string, std::string>::const_iterator map_it;
    for (map_it = req.get_header_map().begin(); map_it != req.get_header_map().end(); map_it++)
    {
        env_key = map_it->first;
        if (map_it->first != "Content-Type" && map_it->first != "Content-Length")
        {
            Utils::replaceAll(env_key);
            Utils::toUpper(env_key);
        }
        _envMap["HTTP_" + env_key] = map_it->second;
    }
    printEnvMap(_envMap);
}

// 4. 主控 Init 函数：结构更加清晰
bool CGIHandler::init(const HttpRequest& req, const RouterCtx& ctx)
{
    _scriptPath = ctx.full_path;

    if (ctx.loc && !ctx.loc->cgi_path.empty()) {
        _binPath = ctx.loc->cgi_path;
    } else {
        return false;
    }
    if (req.get_body_len() > 0) { _inBuffer = req.get_body(); }
    _prepareEnvMap(req);
    _mapToEnvp();
    updateTime();

    return true;
}

bool CGIHandler::execute(const HttpRequest& req)
{
    (void)req;
    // 1. 防御性拦截
    if (_binPath.empty() || _scriptPath.empty()) {
        std::cerr << "[CGI Error] Cannot execute: _binPath or _scriptPath is EMPTY!" << std::endl;
        _state = CGI_ERROR;  // 状态切为 ERROR，方便主循环清理并返回 500
        return false;
    }

    // 2. 安全创建管道 (严格防范 FD 泄漏)
    if (pipe(_pipeIn) < 0) {
        _state = CGI_ERROR;
        return false;
    }
    if (pipe(_pipeOut) < 0) {
        close(_pipeIn[0]);
        close(_pipeIn[1]);
        _pipeIn[0] = -1;
        _pipeIn[1] = -1;
        _state     = CGI_ERROR;
        return false;
    }

    // 3. 创建子进程
    _pid = fork();
    if (_pid < 0) {
        _close_all_pipes();  // 复用我们在析构中设计的全量清理函数
        _state = CGI_ERROR;
        return false;
    }

    // 4. 子进程流 (The Child)
    if (_pid == 0) {
        // 设置信号
        Webserv::resetCgiChildSignals();
        
        // 重定向 STDIN 和 STDOUT
        dup2(_pipeIn[0], STDIN_FILENO);
        dup2(_pipeOut[1], STDOUT_FILENO);

        // 子进程也必须关闭所有用不到的 FD 副本
        close(_pipeIn[0]);
        close(_pipeIn[1]);
        close(_pipeOut[0]);
        close(_pipeOut[1]);

        // 🌟 使用 const_cast 完美解决类型警告，且零内存分配开销
        char* args[3];
        args[0] = const_cast<char*>(_binPath.c_str());
        args[1] = const_cast<char*>(_scriptPath.c_str());
        args[2] = NULL;

        execve(args[0], args, _envp);

        // 如果代码执行到这里，说明 execve 100% 失败了
        std::cerr << "[CGI ERROR] Execve failed: " << strerror(errno) << std::endl;
        exit(1);
    }
    // 5. 父进程流 (The Parent)
    else {
        // 父进程必须无条件关闭子进程使用的两端
        close(_pipeIn[0]);
        _pipeIn[0] = -1;
        close(_pipeOut[1]);
        _pipeOut[1] = -1;

        // 安全地设置非阻塞 I/O
        if (_pipeIn[1] != -1) {
            fcntl(_pipeIn[1], F_SETFL, fcntl(_pipeIn[1], F_GETFL, 0) | O_NONBLOCK);
        }
        if (_pipeOut[0] != -1) {
            fcntl(_pipeOut[0], F_SETFL, fcntl(_pipeOut[0], F_GETFL, 0) | O_NONBLOCK);
        }

        // 🌟 核心：更新状态机，启动 IO 推拉循环
        _state = CGI_EXECUTING;
        updateTime();

        // 针对 GET 请求的优化：如果没有 Body，直接关闭写端，让 CGI 脚本立刻收到 EOF
        if (_inBuffer.empty()) {
            close(_pipeIn[1]);
            _pipeIn[1] = -1;
        }
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
        for (int i = 0; _envp[i] != NULL; i++) {
            free(_envp[i]);
            _envp[i] = NULL;
        }
        delete[] _envp;
    }
}

std::string CGIHandler::getRawResponse()const
{ return _outBuffer; }

int CGIHandler::get_waitpid_status()const
{ return _waitpid_status; }

int CGIHandler::sendToScript()
{
    if (_state != CGI_EXECUTING || _pipeIn[1] == -1) return 0;

    if (_inBuffer.empty()) {
        close(_pipeIn[1]);
        _pipeIn[1] = -1;
        return 0;
    }

    ssize_t bytes_sent = write(_pipeIn[1], _inBuffer.c_str(), _inBuffer.size());
    if (bytes_sent > 0) {
        _inBuffer.erase(0, bytes_sent);
        if (_inBuffer.empty()) {
            close(_pipeIn[1]);
            _pipeIn[1] = -1;
            return 0;
        }
        return 1;  // 还没发完，继续 POLLOUT
    } else if (bytes_sent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        _state = CGI_ERROR;
        _close_all_pipes();
        return -1;
    }
    return 1;
}

int CGIHandler::receiveFromScript()
{
    // 防御性拦截
    if (_state != CGI_EXECUTING || _pipeOut[0] == -1) { return 0; }

    char    buffer[8192];  // 8K 局部缓冲区
    ssize_t bytes_read = read(_pipeOut[0], buffer, sizeof(buffer));
    updateTime();
    
    if (bytes_read > 0) {
        // 防御性编程：检查是否超过服务器配置的 CGI 最大响应限制，防止内存溢出 (OOM)
        // TODO: MAX_CGI_RESPONSE_SIZE 改成conf中的配置
        if (_outBuffer.size() + bytes_read > MAX_CGI_RESPONSE_SIZE) {
            _state = CGI_ERROR;
            _close_all_pipes();
            std::cerr << "[DEBUG] exceed max response size" << std::endl;
            return -1;
        }
        // 安全地追加数据
        _outBuffer.append(buffer, bytes_read);
        return 1;
    } else if (bytes_read == 0) {
        // 管道读到 0 代表脚本已经关闭了输出端，说明数据已经全部安全拉取完毕
        close(_pipeOut[0]);
        _pipeOut[0] = -1;

        if (_pid == -1) //已经被waitpid检查过，_pid 被设置成-1！ 
            _state = CGI_FINISHED;
        // 💡 注意：这里不要盲目地直接把状态改成 CGI_FINISHED！
        // 最佳实践是：数据读完了，但我们依然需要通过 waitpid 确认子进程退出了，
        // 这样可以确保状态的原子性。我们可以在 checkChildProcess() 中做最终的状态切换。
        return 0;
    } else {
        // 非阻塞读取在无数据可读时会返回 -1 且 errno 为 EAGAIN
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            _state = CGI_ERROR;
            _close_all_pipes();
        }
    }
    return -1;
}

int CGIHandler::getPid() const
{ return _pid; }

int CGIHandler::getReadFd() const
{ return _pipeOut[0]; }

int CGIHandler::getWriteFd() const
{ return _pipeIn[1]; }

CGIHandler::CGIState CGIHandler::getState() const
{ return _state; }

void CGIHandler::updateTime()
{ _last_activity_time = std::time(NULL); }

bool CGIHandler::isTimeout()
{
    // 只有在执行状态才需要判断超时
    if (_state != CGI_EXECUTING || _pid <= 0) { return false; }

    if ((std::time(NULL) - _last_activity_time) > CGI_TIMEOUT_SEC) {
        std::cerr << "[CGI Timeout] Process " << _pid << " is taking too long. Killing it."
                  << std::endl;

        // 1. 痛下杀手：强制杀死失控的子进程
        kill(_pid, SIGKILL);

        // 2. 毁尸灭迹：立刻回收僵尸进程
        waitpid(_pid, NULL, 0);
        _pid = -1;

        // 3. 切断连接：关闭所有管道，防止后续读写卡死
        _close_all_pipes();

        // 4. 状态机切换：标记为错误，外层收到后直接返回 504 Gateway Timeout
        _state = CGI_ERROR;

        return true;
    }
    return false;
}

bool CGIHandler::checkChildProcess()
{
    if (_state != CGI_EXECUTING || _pid <= 0) {
        return (_state == CGI_FINISHED);  // 只有真正完成才返回 true
    }
 
    pid_t result = waitpid(_pid, &_waitpid_status, WNOHANG);

    if (result == _pid) {
        // 子进程已退出，我们把它标记为 -1，防止以后重复 waitpid
        _pid = -1;

        // 检查子进程是否是非正常死亡（比如段错误、被信号杀死、或者脚本 exit(1)）
        if (WIFEXITED(_waitpid_status) && WEXITSTATUS(_waitpid_status) != 0) {
            std::cerr << "[CGI Error] Script exited with error code: " << WEXITSTATUS(_waitpid_status)
                      << std::endl;
            _state = CGI_ERROR;
            _close_all_pipes();
            return false;
        }

        if (_pipeOut[0] == -1)
        {
            _state = CGI_FINISHED;
        }
        // 🚨 重点：我们在这里 **绝对不能** 把状态设为 CGI_FINISHED！
        // 因为管道里可能还有数据没读完。
        // 子进程死后，它的管道写端会自动关闭。
        // 这会导致你的 receiveFromScript() 在下一次 epoll 触发时读到 bytes_read == 0。
        // 那里才是真正标志着“数据读完，可以结束”的唯一正确地点！

        return false;
    } else if (result < 0) {
        // waitpid 发生严重错误
        _state = CGI_ERROR;
        _close_all_pipes();
        return false;
    }
    return false;
}

void CGIHandler::_close_all_pipes()
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
    this->_clearEnvp();

    this->_envp = NULL;

    this->_envMap.clear();
    this->_scriptPath.clear();
    this->_binPath.clear();
    this->_inBuffer.clear();
    this->_outBuffer.clear();

    this->_waitpid_status     = 0;
    this->_pid                = -1;
    this->_last_activity_time = 0;
    this->_state              = CGI_FINISHED;

    this->_pipeIn[0]  = -1;
    this->_pipeIn[1]  = -1;
    this->_pipeOut[0] = -1;
    this->_pipeOut[1] = -1;
}