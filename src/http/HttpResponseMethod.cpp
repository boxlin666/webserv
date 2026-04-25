#include "HttpResponse.hpp"

int HttpResponse::_handle_get(void)
{
    int fd;
    ssize_t ret;
    std::vector<char> tmp(this->_body_len);

    fd = open(this->_full_path.c_str(), O_RDONLY);
    if (fd == -1)
        return (NOT_FOUND); 
    if (this->_body_len == 0)
    {
        this->_body.clear();
        close(fd);
        return (SUCCESS);
    }
    ret = read(fd, &tmp[0], this->_body_len);
    close(fd);
    if (ret < 0) 
    {
        this->_body.clear();
        return (SERVER_ERROR);
    }
    this->_body.assign(tmp.begin(), tmp.begin() + ret);
    return (SUCCESS);
}

int HttpResponse::_handle_post(const HttpRequest& req)
{
    int fd;
    ssize_t ret;
    size_t total_size;
    size_t byte_written;
    const char *tmp_buff;

    fd = open(this->_full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
        return (PER_DENIED);
    if (req.get_body_len() == 0)
    {
        this->_body.clear();
        close(fd);
        return (CREATED);
    }
    total_size = req.get_body_len();
    byte_written = 0;
    tmp_buff = req.get_body_content().data(); 
    ret = 0;
    while (1)
    {
        ret = write(fd, tmp_buff + byte_written, total_size - byte_written);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue ;
            close(fd);
            return (SERVER_ERROR);
        }
        if (ret == 0 && total_size > 0)
        {
            close(fd);
            return (SERVER_ERROR);
        }
        byte_written += ret;
        if (byte_written >= total_size)
            break ;
    }
    close(fd);
    return (CREATED);
}

int HttpResponse::_handle_delete(void)
{
    if (std::remove(this->_full_path.c_str()))
        return (DELETED);
    if (errno == ENOENT)
        return (NOT_FOUND);
    else if (errno == EACCES || errno == EPERM)
        return (PER_DENIED);
    return (SERVER_ERROR);
}
