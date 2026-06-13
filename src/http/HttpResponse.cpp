#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include <iostream>

HttpResponse::HttpResponse(void)
{
    this->reset();
}

HttpResponse::~HttpResponse(void)
{

}

void HttpResponse::reset(void)
{
    this->_set_status(SUCCESS); //default OK at the beginning! 
    this->_status_line.clear();
    this->_headers_vector.clear();
    this->_cgi_headers_vector.clear();
    this->_body.clear();
    this->_body_last_modif_date.clear();
    this->_full_response.clear();
    this->_body_len = 0;
    this->_full_path.clear();
}

void HttpResponse::build_static_response(const HttpRequest& request, const RequestHandler& response_ctx, int &status_code)
{
    int ret = status_code;

    this->_status_code = status_code;
    this->_body_last_modif_date = response_ctx.get_body_last_modif_date(); 
    
    //TODO: if status code is not SUCCESS, redefine the content length according to the nb of bytes inside error page html
    if (status_code == SUCCESS)
    { 
        if (request.get_method() == "GET" || request.get_method() == "HEAD" || request.get_method() == "DELETE")
            this->_body_len = response_ctx.get_res_body_len();
    }
    std::cout << "_BODY_LEN = " << this->_body_len << std::endl;

    if (this->_status_code == SUCCESS)
        this->_full_path = response_ctx.get_full_path();

    if (this->_status_code == SUCCESS)
    {
        if (request.get_method() == "GET" || request.get_method() == "HEAD")
            ret = this->_handle_get();
        else if (request.get_method() == "POST")
            ret = this->_handle_post(request);
        else if (request.get_method() == "DELETE")
            ret = this->_handle_delete();
    }

    this->_status_code = ret;

    this->_prepare_response_data(request);

    if (request.get_method() == "HEAD")
        this->_body.clear();

    //6. append all the elements together!
    this->_append_full_response();
}

void HttpResponse::_append_full_response(void)
{
    this->_full_response.reserve(this->_status_line.size() + this->_body.size() + 1024);
    this->_full_response += this->_status_line;

    for (std::vector<HeaderPair>::const_iterator it = this->_headers_vector.begin(); it != this->_headers_vector.end(); ++it)
    {
        this->_full_response += it->first + ": " + it->second + "\r\n";
    }
    this->_full_response += "\r\n";
    this->_full_response.append(this->_body);

    debug_request_msg_print("OUT_BUFF", _full_response);
}
