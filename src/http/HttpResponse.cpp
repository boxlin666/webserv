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
    this->_body.clear();
    this->_body_last_modif_date.clear();
    this->_full_response.clear();
    this->_body_len = 0;
    this->_full_path.clear();
}

void HttpResponse::build(const HttpRequest& request, const RequestHandler& response_ctx, int &status_code)
{
    int ret = status_code;

    this->_status_code = status_code;
    this->_body_last_modif_date = response_ctx.get_body_last_modif_date(); 
    //this->_body_last_modif_date = response_ctx.get_body_last_modif_date();
    this->_body_len = response_ctx.get_res_body_len();
    this->_full_path = response_ctx.get_full_path();

    if (this->_status_code == 200)
    {
        if (request.get_method() == "GET")
            ret = this->_handle_get();
        else if (request.get_method() == "POST")
            ret = this->_handle_post(request, response_ctx);
        else if (request.get_method() == "DELETE")
            ret = this->_handle_delete();
    }

    this->_status_code = ret;
 
    this->_prepare_response_data(request);

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
}

void HttpResponse::build_from_cgi(const std::string& cgi_output)
{
    // 1. 清空上一次请求的残留数据（直接复用你写好的 reset）
    this->reset();

    // 2. 强行设定 CGI 成功的状态码
    this->_status_code = 200;
    this->_status_line = "HTTP/1.1 200 OK\r\n";

    // 3. 构建 HTTP 基础报文
    this->_full_response = this->_status_line;
    this->_full_response += "Server: Webserv/1.0\r\n";
    this->_full_response += "Date: " + this->_generate_date() + "\r\n";

    // 强制关闭：
    this->_full_response += "Connection: close\r\n";
    // 4. 🌟 乾坤大挪移：把 CGI 脚本自己打印的 Header 和 Body 直接接在后面
    // 因为 test.py 吐出的是 "Content-Type: text/html\r\n\r\n<h1>...</h1>"
    this->_full_response += cgi_output;
}