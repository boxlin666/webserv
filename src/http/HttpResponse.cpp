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
    this->_status_msg_map.clear();
    this->_ext_map.clear();
    this->_headers_map.clear();
    this->_body.clear();
    this->_body_last_modif_date.clear();
    this->_full_response.clear();
    this->_body_len = 0;
    this->_full_path.clear();
}

void HttpResponse::build(const RequestHandler& response_ctx, const int &status_code)
{
    int ret;

    //1.prerequiste check! 
    /*ret = this->_check_request(req);
    if (ret != 200)
    {
        this->_set_status(ret);
        //return ;
    }*/

    //(void)config;
    //2. path convert
    //this->_full_path = this->_build_full_path(req, config);
    this->_full_path = response_ctx.get_full_path();
    this->_status_code = status_code;

    //3. check ressource (accessbility)
    /*ret = this->_check_resource(req);

    if (ret != 200):44

    {
        this->_set_status(ret);
        //return ;
    }*/
   
    //4. handle method 
    if (this->_status_code == 200)
    {
        if (response_ctx.get_method_to_apply() == "GET")
            ret = this->_handle_get();
        else if (response_ctx.get_method_to_apply() == "POST")
            ret = this->_handle_post(response_ctx);
        else if (response_ctx.get_method_to_apply() == "DELETE")
            ret = this->_handle_delete();
    }

    if (ret != 200)
    {
        this->_set_status(ret);
        //return ;
    }
    //std::cout << "ret = " << ret << std::endl;
    //5. prepare response data!
    this->_prepare_response_data();

    //6. append all the elements together!
    this->_append_full_response();
}

void HttpResponse::_append_full_response(void)
{
    this->_full_response.reserve(this->_status_line.size() + this->_body.size() + 1024);
    this->_full_response += this->_status_line;
    for (std::map<std::string, std::string>::const_iterator it = this->_headers_map.begin(); it != this->_headers_map.end(); ++it)
    {
        this->_full_response += it->first + ": " + it->second + "\r\n";
    }
    this->_full_response += "\r\n";
    this->_full_response.append(this->_body);
}