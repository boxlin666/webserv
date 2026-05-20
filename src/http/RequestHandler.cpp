#include "RequestHandler.hpp"
#include "HttpConstants.hpp"

RequestHandler::RequestHandler():
_full_path(""),
_body_last_modif_date(""),
_res_body_len(0),
_is_auto_index(false)
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

    int ret = extract_parent_path();
    if (ret != SUCCESS)
    {
        status_code = ret;
        return ;
    }
    status_code = this->dispatch_resource_check(req, route_ctx);
}

int RequestHandler::dispatch_resource_check(const HttpRequest& req, const RouterCtx& route_ctx)
{
    if (req.get_method() == "GET" || req.get_method() == "DELETE" || (req.get_method() == "POST" && route_ctx.is_cgi_potential))
    {
        return (this->existing_resource_validator(route_ctx));
    }
    else if (req.get_method() == "POST" && !route_ctx.is_cgi_potential)
    {
        return (this->creatable_resource_validator());
    }
    return (NO_METHOD);
}

int RequestHandler::existing_resource_validator(const RouterCtx& route_ctx)
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
        return (this->process_directory(route_ctx));
    if (S_ISREG(st.st_mode))
        return (this->process_file(st, this->_full_path));
    return (NOT_FOUND);
}

//Only for "POST static file!!"
int RequestHandler::creatable_resource_validator(void)
{
    struct stat st;
    struct stat st_parent;
    int ret = 0;

    //if we POST a file whose filename has already been recoginized by webserv.
    if (stat(this->_full_path.c_str(), &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return (PER_DENIED);
        if (access(this->_full_path.c_str(), W_OK) != 0)
            return (PER_DENIED);
        return (SUCCESS); //only if request is to overwrite an existing writable file inside server class
    }
  
    //if we POST a file whose filename has not been recoginized by webserv yet.
    ret = this->extract_parent_path();
    if (ret != SUCCESS)
        return (ret);

    if (stat(this->_parent_path.c_str(), &st_parent) == -1)
    {
        if (errno == ENOENT)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (PER_DENIED);
        return (SERVER_ERROR);
    }
    if (!S_ISDIR(st_parent.st_mode)) //KO if _parent_path is not a real parent directory
        return (PER_DENIED);
    if (access(this->_parent_path.c_str(), W_OK) != 0) //We can not write inside this directory
        return (PER_DENIED);
    return (SUCCESS);
}

int RequestHandler::extract_parent_path(void)
{
    std::size_t pos;

    //we suppose that _full_path has already been normalized before...
    pos = this->_full_path.find_last_of('/');
    if (pos == std::string::npos)
        return (BAD_REQUEST);
    else if (pos == 0)
        this->_parent_path = "/";
    else
        this->_parent_path = this->_full_path.substr(0, pos);
    return (SUCCESS);
}

int RequestHandler::process_directory(const RouterCtx& route_ctx)
{
    char last_c;
    struct stat st_index;
    std::string index_file_name;

    last_c = this->_full_path[this->_full_path.size() - 1];
    if (last_c != '/')
        this->_full_path += "/";

    std::vector<std::string> index;

    if (route_ctx.loc)
        index = route_ctx.loc->index;
    else
        index = route_ctx.server->get_index();

    for (std::size_t i = 0; i < index.size(); i++)
    {
        std::string tmp_full_path = this->_full_path + index[i];
        if (stat(tmp_full_path.c_str(), &st_index) == -1)
            continue ;
        if (!S_ISREG(st_index.st_mode))
            continue ;
        if (this->process_file(st_index, tmp_full_path) != SUCCESS)
            continue ;
        else
        {
            this->_full_path = tmp_full_path;
            return (SUCCESS);
        }
    }

    if (route_ctx.loc->autoindex == true)
    {
        this->_is_auto_index = true;
        return (SUCCESS);
    }
    return (NOT_FOUND);
}

int RequestHandler::process_file(const struct stat& st, const std::string &input_path)
{
    if (access(input_path.c_str(), R_OK) == -1)
        return (PER_DENIED);
    this->_body_last_modif_date = Utils::formatHttpDate(st.st_mtime);
    this->_set_res_body_len(st.st_size);
    return (SUCCESS);
}

void RequestHandler::_set_res_body_len(size_t body_len)
{
    this->_res_body_len = body_len;
}

const std::string &RequestHandler::get_full_path()const
{
    return (this->_full_path);
}

std::size_t RequestHandler::get_res_body_len()const
{
    return (this->_res_body_len);
}

const std::string &RequestHandler::get_body_last_modif_date()const
{
    return (this->_body_last_modif_date);
}