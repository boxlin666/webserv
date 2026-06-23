#include "CGIHandler.hpp"

#include "NotificationPipe.hpp"
#include "Utils.hpp"

CGIHandler::CGIHandler() : _pid(-1), _state(CGI_INIT), _last_activity_time(0), _envp(NULL)
{
    _pipeIn[0]  = -1;
    _pipeIn[1]  = -1;
    _pipeOut[0] = -1;
    _pipeOut[1] = -1;
}

CGIHandler::~CGIHandler()
{
    _clearEnvp();
    _close_all_pipes();
}

void CGIHandler::_prepareEnvMap(const HttpRequest& req)
{
    char* raw_path  = std::getenv("PATH");
    _envMap["PATH"] = (raw_path != NULL) ? std::string(raw_path) : "/usr/bin:/bin:/usr/local/bin";

    _envMap["REQUEST_METHOD"]    = req.get_method();
    _envMap["QUERY_STRING"]      = req.get_querystring();
    _envMap["PATH_INFO"]         = req.get_path();
    _envMap["GATEWAY_INTERFACE"] = "CGI/1.1";
    _envMap["SERVER_PROTOCOL"]   = "HTTP/1.1";
    _envMap["SERVER_SOFTWARE"]   = "Webserv/1.0";
    _envMap["REDIRECT_STATUS"]   = "200";

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        _envMap["SCRIPT_FILENAME"] = std::string(cwd) + "/" + _scriptPath;
    else
        _envMap["SCRIPT_FILENAME"] = _scriptPath;

    if (req.get_is_chunked() == false) {
        std::string req_content_length = Utils::toString(req.get_content_length());
        _envMap["CONTENT_LENGTH"]      = req_content_length;
    }

    std::string req_content_type = req.get_header("content-type");

    if (req_content_type.empty())
        _envMap["CONTENT_TYPE"] = "";
    else
        _envMap["CONTENT_TYPE"] = req_content_type;

    std::string                                        env_key;
    std::map<std::string, std::string>::const_iterator map_it;
    for (map_it = req.get_header_map().begin(); map_it != req.get_header_map().end(); map_it++) {
        env_key = map_it->first;
        if (map_it->first != "content-type" && map_it->first != "content-length") {
            Utils::replaceAll(env_key);
            Utils::toUpper(env_key);
            _envMap["HTTP_" + env_key] = map_it->second;
        }
    }
    // printEnvMap(_envMap);
}

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

bool CGIHandler::execute()
{
    if (_binPath.empty() || _scriptPath.empty()) {
        std::cerr << "[CGI Error] Cannot execute: _binPath or _scriptPath is EMPTY!" << std::endl;
        _state = CGI_ERROR;
        return false;
    }
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

    _pid = fork();
    if (_pid < 0) {
        _close_all_pipes();
        _state = CGI_ERROR;
        return false;
    }

    if (_pid == 0) {
        Webserv::resetCgiChildSignals();

        dup2(_pipeIn[0], STDIN_FILENO);
        dup2(_pipeOut[1], STDOUT_FILENO);

        close(_pipeIn[0]);
        close(_pipeIn[1]);
        close(_pipeOut[0]);
        close(_pipeOut[1]);

        char* args[3];
        args[0] = const_cast<char*>(_binPath.c_str());
        args[1] = const_cast<char*>(_scriptPath.c_str());
        args[2] = NULL;

        execve(args[0], args, _envp);
        std::cerr << "[CGI ERROR] Execve failed." << std::endl;
        exit(1);
    } else {
        close(_pipeIn[0]);
        _pipeIn[0] = -1;
        close(_pipeOut[1]);
        _pipeOut[1] = -1;

        if (_pipeIn[1] != -1)
            fcntl(_pipeIn[1], F_SETFL, fcntl(_pipeIn[1], F_GETFL, 0) | O_NONBLOCK);
        if (_pipeOut[0] != -1)
            fcntl(_pipeOut[0], F_SETFL, fcntl(_pipeOut[0], F_GETFL, 0) | O_NONBLOCK);

        _state = CGI_EXECUTING;
        updateTime();

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

std::string CGIHandler::getRawResponse() const
{ return _outBuffer; }

int CGIHandler::get_waitpid_status() const
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
        return 1;
    } else if (bytes_sent == -1) {
        _state = CGI_ERROR;
        _close_all_pipes();
        return -1;
    }
    return 1;
}

int CGIHandler::receiveFromScript()
{
    if (_state != CGI_EXECUTING || _pipeOut[0] == -1) { return 0; }

    char    buffer[8192];
    ssize_t bytes_read = read(_pipeOut[0], buffer, sizeof(buffer));
    updateTime();
    std::cout << "receive from script to update time ??" << std::endl;

    if (bytes_read > 0) {
        if (_outBuffer.size() + bytes_read > MAX_CGI_RESPONSE_SIZE) {
            _state = CGI_ERROR;
            _close_all_pipes();
            std::cout << "killing this program pid:"  << _pid << std::endl;
            kill(_pid, SIGKILL); 
            int kill_status;
            waitpid(_pid, &kill_status, 0);
            _pid = -1;
            return -1;
        }
        _outBuffer.append(buffer, bytes_read);
        return 1;
    } else if (bytes_read == 0) {
        close(_pipeOut[0]);
        _pipeOut[0] = -1;

        if (_pid == -1) _state = CGI_FINISHED;
        return 0;
    } else {
        _state = CGI_ERROR;
        _close_all_pipes();
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
    if (_state != CGI_EXECUTING || _pid <= 0) return false;

    if (_last_activity_time == 0) return false;

    std::cout << "last activity time: " << _last_activity_time << std::endl;
    std::cout << "time now" << std::time(NULL) << std::endl;
    std::cout << "CGI TIMEOUT SEC" << CGI_TIMEOUT_SEC << std::endl;

    if ((std::time(NULL) - _last_activity_time) > CGI_TIMEOUT_SEC) {
        std::cerr << "[CGI Timeout] Process " << _pid << " is taking too long. Killing it."
                  << std::endl;
        kill(_pid, SIGKILL);
        waitpid(_pid, NULL, 0);
        _pid = -1;
        _close_all_pipes();
        _state = CGI_ERROR;

        return true;
    }
    return false;
}

bool CGIHandler::checkChildProcess()
{
    // 如果不是正在执行状态，或者 pid 已经非法，直接返回是否完成
    //if (_state != CGI_EXECUTING || _pid <= 0) return (_state == CGI_FINISHED);

    std::cout << "Before going to the waitpid loop" << std::endl;
    pid_t result;
    // 使用 while 循环，一次性把所有已经结束的子进程全部回收，防止多个子进程同时结束产生僵尸
    while ((result = waitpid(-1, &_waitpid_status, WNOHANG)) > 0) {
        std::cout << "reapeat inside waitpid loop" << std::endl; 
        // 场景 A：刚好回收到了当前 CGI 实例所管理的那个 PID
        if (result == _pid) {
            _pid = -1; // 标记已经回收成功

            // 检查是否是非正常退出（比如退出码不为 0）
            if (WIFEXITED(_waitpid_status) && WEXITSTATUS(_waitpid_status) != 0) {
                std::cerr << "[CGI Error] Script exited with error code: "
                          << WEXITSTATUS(_waitpid_status) << std::endl;
                _state = CGI_ERROR;
                _close_all_pipes();
                return false;
            }
            
            // 检查是否是被信号杀死的（比如你主动 kill 掉的死循环）
            if (WIFSIGNALED(_waitpid_status)) {
                std::cerr << "[CGI Warning] Script was terminated by signal: "
                          << WTERMSIG(_waitpid_status) << std::endl;
                _state = CGI_ERROR; // 或者根据你的业务定义为 FINISHED 
                _close_all_pipes();
                return false;
            }

            // 如果读端管道已经关闭，说明数据读完了，CGI 正式结束
            if (_pipeOut[0] == -1) { 
                _state = CGI_FINISHED; 
            }
        } 
        else {
            // 场景 B：回收到了别的 CGI 子进程
            // 没关系，这里顺手帮别的 CGIHandler 实例把“尸体”收了，防止了别的进程变成 <defunct>
            // 注意：因为这里帮别人收了尸，建议你的全局设计中，
            // 别的 CGIHandler 对象在检查状态时，不仅看 waitpid，还要看它的管道是否读到了 EOF
            std::cout << "[Webserv] 顺手回收了另一个并发的子进程 PID: " << result << std::endl;
        }
    }

    // 处理 waitpid 出错的情况
    if (result < 0 && errno != ECHILD) {
        _state = CGI_ERROR;
        _close_all_pipes();
        return false;
    }

    return (_state == CGI_FINISHED);
}

void CGIHandler::_close_all_pipes()
{
    if (this->_pipeIn[0] != -1) {
        close(this->_pipeIn[0]);
        this->_pipeIn[0] = -1;
    }
    if (this->_pipeIn[1] != -1) {
        close(this->_pipeIn[1]);
        this->_pipeIn[1] = -1;
    }

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
