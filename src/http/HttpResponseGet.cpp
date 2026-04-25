#include "HttpResponse.hpp"

const std::string& HttpResponse::get_full_response(void)const
{
    return (this->_full_response);
}