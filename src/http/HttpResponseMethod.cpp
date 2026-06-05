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

int HttpResponse::_handle_post(const HttpRequest& request) 
{
    std::size_t pos = this->_full_path.find_last_of('/');

    if (pos == this->_full_path.size() - 1)
        return (this->_handle_static_post_dir());

    return (this->_handle_static_post(request));
}

int HttpResponse::_handle_static_post_dir(void)
{
    if (mkdir(this->_full_path.c_str(), 0755) == 0) 
    {
        _status_code = CREATED;
        return (_status_code);
    }

    if (errno == EEXIST) 
    {
       _status_code = SUCCESS;
       return (_status_code);
    }
    return (SERVER_ERROR);
}

int HttpResponse::_handle_static_post(const HttpRequest& request)
{
    int fd;
    ssize_t ret;
    size_t total_size;
    size_t byte_written;
    const std::string& req_body = request.get_body();
    bool is_overwrite = false;

    fd = open(this->_full_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1)
    {
        if (errno == EEXIST)
        {
            is_overwrite = true;
            fd = open(this->_full_path.c_str(), O_WRONLY | O_TRUNC, 0644);
            if (fd == -1)
            {
                if (errno == EACCES) return (PER_DENIED);
                return (SERVER_ERROR);
            }
        }
        else if (errno == EACCES) return (PER_DENIED);
        else if (errno == ENOENT) return (NOT_FOUND);
        else return (SERVER_ERROR);
    }
    if (!is_overwrite) this->_status_code = CREATED;
    total_size = request.get_body_len();
    if (total_size == 0)
    {
        close(fd);
        return (_status_code);
    }
    byte_written = 0;
    ret = 0;
    while (byte_written < total_size)
    {
        ret = write(fd, req_body.data() + byte_written, total_size - byte_written);
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
    return (_status_code);
}

int HttpResponse::_handle_delete(void)
{
    if (std::remove(this->_full_path.c_str()) == 0)
        return (DELETED);
    if (errno == ENOENT)
        return (NOT_FOUND);
    else if (errno == EACCES || errno == EPERM)
        return (PER_DENIED);
    return (SERVER_ERROR);
}
