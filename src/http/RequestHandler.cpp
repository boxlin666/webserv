#include "RequestHandler.hpp"
#include "HttpConstants.hpp"

RequestHandler::RequestHandler():
_full_path(""),
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
            //std::cout << "req content length = " << req.get_content_length() << std::endl;
            //std::cout << "==============Request Handler ISSUE!!!=================" << std::endl;
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
    status_code = this->check_resource(req, route_ctx);
    //status_code = this->check_cgi_bin_path(route_ctx);
}

int RequestHandler::check_cgi_bin_path(const RouterCtx &route_ctx)const
{
    if (access(route_ctx.loc->cgi_path.c_str(), X_OK) != 0)
        return (SERVER_ERROR);
    return (SUCCESS);
}

int RequestHandler::extract_parent_path(void)
{
    std::size_t pos;

    //we suppos that _full_path has already been normalized before...
    pos = this->_full_path.find_last_of('/');
    if (pos == std::string::npos)
        return (BAD_REQUEST);
    this->_parent_path = this->_full_path.substr(0, pos);
    return (SUCCESS);
}

//Only works on the cmd below:
// curl -v -X POST http://localhost:8080/uploads/post_test --data-binary "@/home/yanzhao/post_test"
// Doesn't work on:
//curl -v -F "file=@/home/yanzhao/post_test" http://localhost:8080/uploads (200, but no upload file created!) 
int RequestHandler::validate_post_target()const
{
    struct stat st;

    if (stat(this->_parent_path.c_str(), &st) == -1)
    {
        if (errno == ENOENT)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (PER_DENIED);
        return (SERVER_ERROR);
    }
    if (S_ISDIR(st.st_mode))
        return (SUCCESS);
    return (NOT_FOUND);
}

int RequestHandler::check_resource(const HttpRequest& req, const RouterCtx& route_ctx)
{
    struct stat st;

    if (stat(this->_full_path.c_str(), &st) == -1)
    {
        if (req.get_method() == "POST")
            return (this->validate_post_target());
         if (errno == ENOENT)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (PER_DENIED);
        return (SERVER_ERROR);
    }
    if (S_ISDIR(st.st_mode))
    {
        if (req.get_method() == "POST")
            return (this->validate_post_target());
        return (this->process_directory(req, route_ctx));
    }
    if (S_ISREG(st.st_mode))
        return (this->process_file(st, this->_full_path));
    return (NOT_FOUND);
}

int RequestHandler::process_directory(const HttpRequest& req, const RouterCtx& route_ctx)
{
    char last_c;
    struct stat st_index;
    std::string index_file_name;

    last_c = this->_full_path[this->_full_path.size() - 1];
    if (req.get_method() != "GET")
    {
        return (METHOD_NOT_ALLOWED);
    }
    if (last_c != '/')
        this->_full_path += "/";
    //this->_full_path += "index.html";

    std::vector<std::string> index;

    if (route_ctx.loc)
        index = route_ctx.loc->index;
    else
        index = route_ctx.server->get_index();

    for (std::size_t i = 0; i < index.size(); i++)
    {
        std::string tmp_full_path = this->_full_path + index[i];
        if (stat(tmp_full_path.c_str(), &st_index) == -1)
        {
            /*if (errno == ENOENT)
                return (NOT_FOUND);
            return (PER_DENIED);*/
            continue ;
        }
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
    //TODO check autoindex!
    return (PER_DENIED);

    /*if (stat(this->_full_path.c_str(), &st_index) == -1)
    {
        if (errno == ENOENT)
            return (NOT_FOUND);
        return (PER_DENIED);
    }
    if (!S_ISREG(st_index.st_mode))
        return (NOT_FOUND);
    return (this->process_file(st_index));*/
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