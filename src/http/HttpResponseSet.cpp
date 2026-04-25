#include "HttpResponse.hpp"

void HttpResponse::_set_status(int code)
{
    this->_status_code = code;
}

void HttpResponse::_set_body_len(size_t body_len)
{
    this->_body_len = body_len;
}
