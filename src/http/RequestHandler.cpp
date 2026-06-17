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
        if (req.get_body_len() > route_ctx.loc->client_max_body_size)
        {
            status_code = BODY_TOO_LARGE ;
            return ;
        }
    }
    this->_full_path = route_ctx.full_path;
    this->_is_cgi_mode = route_ctx.is_cgi_potential;
    this->_multipart_filename = req.get_multipart_filename();

    this->_req_body = req.get_body(); // 确保你有这个成员变量      
    std::cout << "[Debug] Handler synced body. Size: " << this->_res_body_len << std::endl;
    
    status_code = this->dispatch_resource_check(req, route_ctx);
}

int RequestHandler::dispatch_resource_check(const HttpRequest& req, const RouterCtx& route_ctx)
{
    if (route_ctx.is_cgi_potential)
    {
        return (this->cgi_resource_validator(route_ctx));
    }
    if (req.get_method() == "GET" || req.get_method() == "HEAD" || req.get_method() == "DELETE")
    {
        return (this->existing_resource_validator(route_ctx));
    }
    else if (req.get_method() == "POST" && !route_ctx.is_cgi_potential)
    {
        return (this->creatable_resource_validator());
    }
    return (NO_METHOD);
}

int RequestHandler::cgi_resource_validator(const RouterCtx& route_ctx)
{
    // 1. 从配置中获取该后缀对应的 CGI 二进制文件路径（例如 ./cgi-bin/cgi_tester）
    std::string cgi_executable = route_ctx.loc->cgi_path; 

    if (cgi_executable.empty())
        return (NOT_FOUND);

    // 2. 验证这个“执行程序”是否真的存在且可执行
    struct stat st;
    if (stat(cgi_executable.c_str(), &st) == -1)
    {
        // 如果配置的 cgi_tester 找不到了，报 500 或 404
        return (NOT_FOUND); 
    }

    // 3. 只要执行程序在，哪怕 youpla.bla 本身不存在，我们也返回 SUCCESS
    // 这样接下来的 execute_cgi_pipeline 就会被触发
    if (access(cgi_executable.c_str(), X_OK) == 0 && access(this->_full_path.c_str(), R_OK) == 0)
        return (SUCCESS);

    return (FORBIDDEN);
}
 
int RequestHandler::existing_resource_validator(const RouterCtx& route_ctx)
{
    struct stat st;

    if (stat(this->_full_path.c_str(), &st) == -1)
    {
        std::cerr << "[DEBUG] stat failed: " << this->_full_path 
              << " errno: " << strerror(errno) << std::endl;
        if (errno == ENOENT)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (FORBIDDEN);
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
        if (access(this->_full_path.c_str(), W_OK) != 0)
            return (FORBIDDEN);
    }

    ret = this->check_ext_post_file();
    if (ret!= SUCCESS)
        return (ret);
  
    //if we POST a file whose filename has not been recoginized by webserv yet.
    ret = this->extract_parent_path();
    if (ret != SUCCESS)
        return (ret);

    if (stat(this->_parent_path.c_str(), &st_parent) == -1)
    {
        if (errno == ENOENT)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (FORBIDDEN);
        return (SERVER_ERROR);
    }
    if (!S_ISDIR(st_parent.st_mode)) //KO if _parent_path is not a real parent directory
        return (FORBIDDEN);
    if (access(this->_parent_path.c_str(), W_OK) != 0) //We can not write inside this directory
        return (FORBIDDEN);
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
        this->_parent_path = this->_full_path.substr(0, pos + 1);
    //POST  
    if (pos + 1 == this->_full_path.size() && _multipart_filename.empty())
    {
        std::string file_name = "post_body" + Utils::generate_unique_id();

        this->_full_path += file_name;
    }
    if (!_multipart_filename.empty())
        this->_full_path += _multipart_filename;
    return (SUCCESS);
}

int RequestHandler::check_ext_post_file(void)const
{
    std::size_t pos = this->_full_path.find_last_of('/');
    
    if (pos != std::string::npos)
    {
        std::string file_name = this->_full_path.substr(pos + 1, this->_full_path.length());
        pos = file_name.find_last_of('.');
        if (pos != std::string::npos)
        {
            std::string file_name_ext = file_name.substr(pos, file_name.length()); 
            if (file_name_ext == ".html" || file_name_ext == ".py")
                return (METHOD_NOT_ALLOWED);
        }
    }
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
        return (FORBIDDEN);
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

// TODO: just for test
std::string RequestHandler::get_req_body()const
{
    return (this->_req_body);
}

bool RequestHandler::is_auto_index() const
{
    return _is_auto_index;
}

bool RequestHandler::is_cgi_mode()const
{
    return (this->_is_cgi_mode);
}

const std::string &RequestHandler::get_body_last_modif_date()const
{
    return (this->_body_last_modif_date);
}