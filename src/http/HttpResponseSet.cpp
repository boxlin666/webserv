#include "HttpResponse.hpp"

void HttpResponse::_set_status(int code)
{
    if (code < 100 || code > 599)
    {
        this->_status_code = 500;
        return ;
    }   
    if (this->_status_code >= 400 && code < 400)
        return ;
    this->_status_code = code;
}

void HttpResponse::_set_body_len(size_t body_len)
{
    this->_body_len = body_len;
}
