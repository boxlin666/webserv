#include "HttpResponse.hpp"

std::map<int, std::string> HttpResponse::_status_msg_map;
std::map<std::string, std::string> HttpResponse::_ext_map;

void HttpResponse::init_response_map(void)
{
    HttpResponse::_init_ext_map();
    HttpResponse::_init_status_msg_map();
}

void    HttpResponse::_init_status_msg_map(void) 
{
    /* 2xx */
    HttpResponse::_status_msg_map[SUCCESS] = "OK";
    HttpResponse::_status_msg_map[CREATED] = "Created";
    HttpResponse::_status_msg_map[DELETED] = "No Content";

    /* 4xx */
    HttpResponse::_status_msg_map[BAD_REQUEST] = "Bad Request";
    HttpResponse::_status_msg_map[PER_DENIED] = "Permission Denied"; 
    HttpResponse::_status_msg_map[NOT_FOUND] = "Forbidden";

    /* 5xx */
    HttpResponse::_status_msg_map[METHOD_NOT_ALLOWED] = "Method Not Allowed";
    HttpResponse::_status_msg_map[BODY_TOO_LARGE] = "Content Too Large";
    HttpResponse::_status_msg_map[URI_TOO_LONG] = "URI Too Long";
    HttpResponse::_status_msg_map[SERVER_ERROR] = "Internal Server Error";
    HttpResponse::_status_msg_map[NO_METHOD] = "Not Implemented";
    HttpResponse::_status_msg_map[NO_HTTP_VERSION] = "HTTP Version Not Supported";
}

void    HttpResponse::_init_ext_map(void)
{
    if (!HttpResponse::_ext_map.empty())
        return ;
    HttpResponse::_ext_map["html"] = "text/html";
    HttpResponse::_ext_map["css"]  = "text/css";
    HttpResponse::_ext_map["js"]   = "text/javascript";
    HttpResponse::_ext_map["jpg"]  = "image/jpeg";
    HttpResponse::_ext_map["jpeg"] = "image/jpeg";
    HttpResponse::_ext_map["png"]  = "image/png";
    HttpResponse::_ext_map["gif"]  = "image/gif";
    HttpResponse::_ext_map["txt"]  = "text/plain";
    HttpResponse::_ext_map["ico"]  = "image/x-icon";
}