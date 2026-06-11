// NotificationPipe.hpp
#ifndef NOTIFICATION_PIPE_HPP
#define NOTIFICATION_PIPE_HPP

#include <fcntl.h>
#include <unistd.h>

#include <csignal>

namespace Webserv {
class NotificationPipe {
   private:
    int _pipe_fds[2];
    NotificationPipe(const NotificationPipe& other);
    NotificationPipe& operator=(const NotificationPipe& other);

   public:
    NotificationPipe();
    ~NotificationPipe();
    bool init();
    int  getReadFd() const;
    void notifyFromHandler();
    void clearNotification();
};
extern NotificationPipe* g_signal_bridge;
void                     cSignalHandler(int signum);
void                     resetCgiChildSignals();
}  // namespace Webserv

#endif