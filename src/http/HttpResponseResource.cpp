#include "HttpResponse.hpp"

int HttpResponse::_check_request(const HttpRequest& req)const
{
    (void)_config;
    size_t  req_line_len = 0;
    size_t  req_header_len = 0;
    std::string request_path = "";

    req_line_len = req.get_method().length() + req.get_path().length() + req.get_version().length();

    for (std::map<std::string, std::string>::const_iterator it = req.get_header_map().begin(); it != req.get_header_map().end(); ++it)
    {
        req_header_len += it->first.length() + it->second.length() + 4; //CTRL
    }
    if (req_line_len > URI_SIZE)
        return (URI_TOO_LONG);
    if (req_header_len > MAX_HEADER_SIZE)
        return (MAX_HEADER_SIZE);
    if (req.get_version() != "HTTP/1.1")
        return (NO_HTTP_VERSION);
    if (req.get_method() != "GET" && req.get_method() != "POST" && req.get_method() != "DELETE")
        return (NO_METHOD);
    if (req.get_body_len() > 100000000000000000 && req.get_method() == "POST") //temporary limit should be set up by config file!
        return (BODY_TOO_LARGE);
    request_path = req.get_path();
    if (request_path[0] != '/' || request_path.empty())
        return (BAD_REQUEST);
    /* TO DO
        if find /../../ in path we return BAD_REQUEST as well
    */ 
    return (200); //assume the check is OK! But we can still have 403 (permission denied) 404 （not found）
}

std::string HttpResponse::_build_full_path(const HttpRequest& req, const ServerConfig& config)const
{
    (void)config;
    //we assume _current_path ending without "\" (alreamd trimmed in Config class!)
    std::string root_path; //should be found in config!
    std::string full_path;
    char    buffer[100]; //tmp 

    getcwd(buffer, 100); //tmp because we don't have config!
    root_path = buffer; //tmp 
    root_path += "/www"; //tmp
    full_path = root_path + req.get_path(); //tmp funct!

    return (full_path);
}

int HttpResponse::_check_resource(const HttpRequest& req)
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
        return (this->_process_directory(req));
    if (S_ISREG(st.st_mode))
        return (this->_process_file(st));
    return (NOT_FOUND);
}

int HttpResponse::_process_directory(const HttpRequest& req)
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
    return (this->_process_file(st_index));
}

int HttpResponse::_process_file(const struct stat& st)
{
    if (access(this->_full_path.c_str(), R_OK) == -1)
        return (PER_DENIED);
    this->_body_last_modif_date = Utils::formatHttpDate(st.st_mtime);
    this->_set_body_len(st.st_size);
    return (SUCCESS);
}
