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
    if (_pipe_fds[1] != -1) { write(_pipe_fds[1], "W", 1); }
}

void NotificationPipe::clearNotification()
{
    char buffer[128];
    while (read(_pipe_fds[0], buffer, sizeof(buffer)) > 0) {}
}

void cSignalHandler(int signum)
{
    if ((signum == SIGINT || signum == SIGTERM) && g_signal_bridge != NULL) {
        g_signal_bridge->notifyFromHandler();
    }
}

void resetCgiChildSignals()
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = SIG_DFL;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
}
}  // namespace Webserv
