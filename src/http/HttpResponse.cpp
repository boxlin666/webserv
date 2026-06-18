#include "HttpResponse.hpp"

#include <iostream>

#include "HttpRequest.hpp"

#include "HttpRequest.hpp"

HttpResponse::HttpResponse(void)
{ this->reset(); }

HttpResponse::~HttpResponse(void) {}

void HttpResponse::reset(void)
{
    this->_set_status(SUCCESS);  // default OK at the beginning!
    this->_status_line.clear();
    this->_headers_vector.clear();
    this->_cgi_headers_vector.clear();
    this->_body.clear();
    this->_body_last_modif_date.clear();
    this->_full_response.clear();
    this->_body_len = 0;
    this->_full_path.clear();
}

bool HttpResponse::build_static_response(const HttpRequest&    request,
                                         const RequestHandler& response_ctx, int& status_code)
{
    int ret = status_code;

    this->_status_code          = status_code;
    this->_body_last_modif_date = response_ctx.get_body_last_modif_date();

    if (status_code == SUCCESS) {
        if (request.get_method() == "GET" || request.get_method() == "HEAD" ||
            request.get_method() == "DELETE")
            this->_body_len = response_ctx.get_res_body_len();
    }

    if (this->_status_code == SUCCESS) this->_full_path = response_ctx.get_full_path();

    if (this->_status_code == SUCCESS) {
        if (request.get_method() == "GET" || request.get_method() == "HEAD") {
            bool autoindex = response_ctx.is_auto_index();
            if (_is_directory(this->_full_path))
                ret = this->_handle_directory(request, autoindex);
            else
                ret = this->_handle_get();
        } else if (request.get_method() == "POST")
            ret = this->_handle_post(request);
        else if (request.get_method() == "DELETE")
            ret = this->_handle_delete();
    }

    this->_status_code = ret;

    if (this->_status_code != SUCCESS) 
        return (false);

    this->_prepare_response_data(request);

    if (request.get_method() == "HEAD") this->_body.clear();

    this->_append_full_response();
    return (true);
}

std::string& HttpResponse::build_error_response(int& status_code, std::string& error_page_path,
                                                HttpRequest& request)
{
    _status_code = status_code;
    _body.clear();
    _full_response.clear();
    if (!error_page_path.empty()) {
        // 读取文件内容作为 body
        std::ifstream file(error_page_path.c_str());
        if (file.is_open()) {
            std::ostringstream ss;
            ss << file.rdbuf();
            _body = ss.str();
        }
    } else {
        _body = "<!DOCTYPE html>\n"
                        "<html lang=\"en\">\n"
                        "<head>\n"
                        "    <meta charset=\"UTF-8\">\n"
                        "    <title>" + Utils::toString(_status_code) + " " + _status_msg_map[_status_code] + "</title>\n"
                        "    \n"
                        "    <link rel=\"icon\" href=\"data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQEAMAAAB6ZgTTAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAwUExURQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAN96vjgAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAASTURBVBjTY2AAA8YwAhIDwQCFAAW7gA3bAAAAAElFTkSuQmCC\" />\n"
                        "    <style>\n"
                        "        body { text-align: center; padding: 150px; font-family: sans-serif; background: #fafafa; color: #333; }\n"
                        "        h1 { font-size: 50px; margin: 0; color: #e74c3c; }\n"
                        "        p { font-size: 20px; color: #666; margin-top: 10px; }\n"
                        "        hr { max-width: 400px; border: 0; border-top: 1px solid #ddd; margin: 20px auto; }\n"
                        "        address { font-style: normal; color: #999; font-size: 14px; }\n"
                        "    </style>\n"
                        "</head>\n"
                        "<body>\n"
                        "    <h1>" + Utils::toString(_status_code) + "</h1>\n"
                        "    <p>" + _status_msg_map[_status_code] + "</p>\n"
                        "    <hr>\n"
                        "    <address>42 Webserv</address>\n"
                        "</body>\n"
                        "</html>";
    }
    _body_len = _body.size();
    _prepare_response_data(request);
    _append_full_response();
    return _full_response;
}

void HttpResponse::_append_full_response(void)
{
    this->_full_response.reserve(this->_status_line.size() + this->_body.size() + 1024);
    this->_full_response += this->_status_line;

    for (std::vector<HeaderPair>::const_iterator it = this->_headers_vector.begin();
         it != this->_headers_vector.end(); ++it) {
        this->_full_response += it->first + ": " + it->second + "\r\n";
    }
    this->_full_response += "\r\n";
    this->_full_response.append(this->_body);

    debug_msg_print("RESPONSE_MSG", _full_response, "\033[32m", 400);
}

int HttpResponse::_handle_directory(const HttpRequest& request, bool is_auto_index)
{
    // 找 index 文件（根据配置的 index 文件名）
    std::string index_path = this->_full_path + "/index.html";
    struct stat st;
    if (stat(index_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        this->_full_path = index_path;
        int ret          = this->_handle_get();
        this->_body_len  = this->_body.size();
        return ret;
    }

    if (is_auto_index) {
        this->_body     = _generate_autoindex(this->_full_path, request.get_path());
        this->_body_len = this->_body.size();
        this->_add_header_vector("Content-Type", "text/html; charset=utf-8");
        return SUCCESS;
    }

    return FORBIDDEN;
}

bool HttpResponse::_is_directory(const std::string& path) const
{
    struct stat st;

    if (stat(path.c_str(), &st) == -1) return false;
    return S_ISDIR(st.st_mode);
}

std::string HttpResponse::_generate_autoindex(const std::string& dir_path, const std::string& uri)
{
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return "";

    std::string html;
    html += "<html>\r\n";
    html += "<head><title>Index of " + uri + "</title></head>\r\n";
    html += "<body>\r\n";
    html += "<h1>Index of " + uri + "</h1><hr>\r\n";
    html += "<pre>\r\n";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".") continue;
        if (entry->d_type == DT_DIR) name += "/";
        html += "<a href=\"" + name + "\">" + name + "</a>\r\n";
    }
    closedir(dir);

    html += "</pre><hr>\r\n";
    html += "</body></html>\r\n";
    return html;
}