#include "RequestHandler.hpp"
#include "HttpConstants.hpp"

RequestHandler::RequestHandler():
_full_path(""),
_method_to_apply(""),
_is_keep_alive(true), //by default we considere request be keep alive in HTTP/1.1
_req_body(NULL),
_req_body_len(0),
_body_last_modif_date(""),
_res_body_len(0)
{
}

RequestHandler::~RequestHandler(void)
{

}

void RequestHandler::process_request_handler(const HttpRequest &req, const RouterCtx &route_ctx, int &status_code)
{
    if (route_ctx.loc)    
    {
        if (req.get_content_length() > route_ctx.loc->client_max_body_size)
        { 
            status_code = BODY_TOO_LARGE ;
            return ;
        }
    }
    this->_full_path = route_ctx.full_path;
    this->_method_to_apply = req.get_method();

    std::map<std::string, std::string>::const_iterator it;
    it = req.get_header_map().find("Connection");
    if (it != req.get_header_map().end())
    {
        if (it->second == "close")
            this->_is_keep_alive = false; 
    }
    this->_req_body = &req.get_body();
    this->_req_body_len = req.get_body_len();
    status_code = this->check_resource(req); 
}

int RequestHandler::check_resource(const HttpRequest& req)
{
    struct stat st;

    if (stat(this->_full_path.c_str(), &st) == -1)
    {
        if (errno == ENOENT)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (PER_DENIED);
        return (SERVER_ERROR);
    }
    if (S_ISDIR(st.st_mode))
        return (this->process_directory(req));
    if (S_ISREG(st.st_mode))
        return (this->process_file(st));
    return (NOT_FOUND);
}

int RequestHandler::process_directory(const HttpRequest& req)
{
    char last_c;
    struct stat st_index;

    last_c = this->_full_path[this->_full_path.size() - 1];
    if (req.get_method() != "GET")
        return (METHOD_NOT_ALLOWED);
    if (last_c != '/')
        this->_full_path += "/";
    this->_full_path += "index.html";
    if (stat(this->_full_path.c_str(), &st_index) == -1)
    {
        if (errno == ENOENT)
            return (NOT_FOUND);
        return (PER_DENIED);
    }
    if (!S_ISREG(st_index.st_mode))
        return (NOT_FOUND);
    return (this->process_file(st_index));
}

int RequestHandler::process_file(const struct stat& st)
{
    if (access(this->_full_path.c_str(), R_OK) == -1)
        return (PER_DENIED);
    this->_body_last_modif_date = Utils::formatHttpDate(st.st_mtime);
    std::cout << "body LAST MODIF DATE" << _body_last_modif_date;
    std::cout << "st.st_size = " << st.st_size << std::endl;
    this->_set_body_len(st.st_size);
    return (SUCCESS);
}

void RequestHandler::_set_body_len(size_t body_len)
{
    this->_res_body_len = body_len;
    std::cout << "_res_body_len " << _res_body_len << std::endl;
}

const std::string &RequestHandler::get_method_to_apply()const
{
    return (this->_method_to_apply);
}

const std::string &RequestHandler::get_full_path()const
{
    return (this->_full_path);
}

const std::string *RequestHandler::get_req_body()const
{
    return (this->_req_body);
}

std::size_t RequestHandler::get_req_body_len()const
{
    return (this->_req_body_len);
}

std::size_t RequestHandler::get_res_body_len()const
{
    return (this->_res_body_len);
}

const std::string &RequestHandler::get_body_last_modif_date()const
{
    return (this->_body_last_modif_date);
}