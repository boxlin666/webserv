#include "NotificationPipe.hpp"

namespace Webserv {
NotificationPipe* g_signal_bridge = NULL;

NotificationPipe::NotificationPipe()
{
    _pipe_fds[0] = -1;
    _pipe_fds[1] = -1;
}

NotificationPipe::~NotificationPipe()
{
    if (_pipe_fds[0] != -1) close(_pipe_fds[0]);
    if (_pipe_fds[1] != -1) close(_pipe_fds[1]);
}

bool NotificationPipe::init()
{
    if (pipe(_pipe_fds) < 0) { return false; }
    fcntl(_pipe_fds[0], F_SETFL, O_NONBLOCK);
    fcntl(_pipe_fds[1], F_SETFL, O_NONBLOCK);
    return true;
}

int NotificationPipe::getReadFd() const
{ return _pipe_fds[0]; }

void NotificationPipe::notifyFromHandler()
{
    // 'W' for Write/Wakeup. write() is async-signal-safe.
    // We don't check the return value extensively here to maintain speed in handler.
    if (_pipe_fds[1] != -1) {
        std::size_t bytes_written = ::write(_pipe_fds[1], "W", 1);
        (void)bytes_written;
    }
}

void NotificationPipe::clearNotification()
{
    char buffer[128];
    // Drain the pipe completely since it's non-blocking
    while (::read(_pipe_fds[0], buffer, sizeof(buffer)) > 0) {
        // Keep reading until empty (EAGAIN / EWOULDBLOCK)
    }
}

void cSignalHandler(int signum)
{
    if ((signum == SIGINT || signum == SIGTERM) && g_signal_bridge != NULL) {
        g_signal_bridge->notifyFromHandler();
    }
}
}  // namespace Webserv