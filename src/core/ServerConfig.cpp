#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
    : _listen_port(8080), _host("127.0.0.1"), _client_max_body_size(1048576)  // 默认 1MB
{ _init_handlers(); }

ServerConfig::~ServerConfig() {}

void ServerConfig::setPort(const std::string& port_str)
{
    for (size_t i = 0; i < port_str.length(); ++i) {
        if (!std::isdigit(port_str[i])) {
            throw std::runtime_error("Invalid character in port: " + port_str);
        }
    }
    long val = std::atol(port_str.c_str());

    if (val < 0 || val > 65535) {
        throw std::runtime_error("Port out of range [0-65535]: " + port_str);
    }

    this->_listen_port = static_cast<int>(val);
}

void ServerConfig::_init_handlers()
{
    _handler_map["root"]                 = &ServerConfig::_handle_root;
    _handler_map["methods"]              = &ServerConfig::_handle_methods;
    _handler_map["autoindex"]            = &ServerConfig::_handle_autoindex;
    _handler_map["cgi_param"]            = &ServerConfig::_handle_cgi;
    _handler_map["listen"]               = &ServerConfig::_handle_listen;
    _handler_map["server_name"]          = &ServerConfig::_handle_server_name;
    _handler_map["root"]                 = &ServerConfig::_handle_root;
    _handler_map["index"]                = &ServerConfig::_handle_index;
    _handler_map["error_page"]           = &ServerConfig::_handle_error_page;
    _handler_map["client_max_body_size"] = &ServerConfig::_handle_client_max_body_size;
    _handler_map["return"]               = &ServerConfig::_handle_return;
    _handler_map["upload_path"]          = &ServerConfig::_handle_upload_path;
    _handler_map["cgi_path"]             = &ServerConfig::_handle_cgi_path;
    _handler_map["cgi_ext"]              = &ServerConfig::_handle_cgi_ext;
}

bool ServerConfig::hasHandler(const std::string& directive) const
{ return _handler_map.find(directive) != _handler_map.end(); }

void ServerConfig::executeHandler(const std::string& directive, std::vector<Token>& tokens,
                                  size_t& pos, location* loc)
{
    LocationHandler handler = _handler_map[directive];
    (this->*handler)(tokens, pos, loc);
}

void ServerConfig::_handle_root(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'root'");
    }

    std::string path = tokens[pos].content;

    if (loc == NULL) {
        // 当前在解析 server 块，存给 ServerConfig 自己的成员变量
        this->_root = path;
    } else {
        // 当前在解析 location 块，存给 location 结构体
        loc->root = path;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_autoindex(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size()) throw std::runtime_error("Missing value for autoindex");

    bool is_on = (tokens[pos].content == "on");

    if (loc == NULL) {
        // 如果 Nginx 允许在 server 级别开 autoindex，就存在 Server 里（可选）
        // this->_autoindex = is_on;
    } else {
        loc->autoindex = is_on;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_cgi(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    // CGI 通常只能写在 location 里
    if (loc == NULL) {
        throw std::runtime_error("'cgi_param' directive is only allowed in location block");
    }

    if (++pos >= tokens.size()) throw std::runtime_error("Missing extension for cgi_param");
    std::string ext = tokens[pos].content;  // 比如 ".py"

    if (++pos >= tokens.size()) throw std::runtime_error("Missing executable path for cgi_param");
    std::string path = tokens[pos].content;  // 比如 "/usr/bin/python3"

    loc->_cgi_param[ext] = path;

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_listen(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (loc != NULL) {
        throw std::runtime_error(
            "Syntax error: 'listen' directive is not allowed in location block");
    }

    if (++pos >= tokens.size()) {
        throw std::runtime_error("Syntax error: Missing port number for 'listen'");
    }

    this->setPort(tokens[pos].content.c_str());

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_server_name(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (loc != NULL) {
        throw std::runtime_error(
            "Syntax error: 'server_name' directive is not allowed in location block");
    }

    if (pos + 1 >= tokens.size() || tokens[pos + 1].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'server_name'");
    }

    // 循环读取多个域名 (如 server_name a.com b.com;)
    while (++pos < tokens.size() && tokens[pos].content != ";") {
        this->_server_names.push_back(tokens[pos].content);
    }

    if (pos >= tokens.size()) {
        throw std::runtime_error("Syntax error: Missing ';' after server_name");
    }
    Utils::expect_semicolon(tokens, pos);
}

void ServerConfig::_handle_index(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (pos + 1 >= tokens.size() || tokens[pos + 1].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'index'");
    }

    while (++pos < tokens.size() && tokens[pos].content != ";") {
        if (loc == NULL) {
            this->_index.push_back(tokens[pos].content);
        } else {
            loc->index.push_back(tokens[pos].content);
        }
    }

    Utils::expect_semicolon(tokens, pos);
}

void ServerConfig::_handle_error_page(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size()) {
        throw std::runtime_error("Syntax error: Missing arguments for 'error_page'");
    }

    std::vector<int> codes;

    while (pos < tokens.size() && std::isdigit(tokens[pos].content[0])) {
        codes.push_back(std::atoi(tokens[pos].content.c_str()));
        pos++;
    }

    if (pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing file path for 'error_page'");
    }
    if (codes.empty()) {
        throw std::runtime_error("Syntax error: Missing status code(s) for 'error_page'");
    }

    std::string error_file_path = tokens[pos].content;

    for (size_t i = 0; i < codes.size(); ++i) {
        if (loc == NULL) {
            this->_error_pages[codes[i]] = error_file_path;
        } else {
            loc->error_pages[codes[i]] = error_file_path;
        }
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_client_max_body_size(std::vector<Token>& tokens, size_t& pos,
                                                location* loc)
{
    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'client_max_body_size'");
    }

    std::string val        = tokens[pos].content;
    long long   multiplier = 1;
    char        last_char  = val[val.length() - 1];

    if (last_char == 'k' || last_char == 'K')
        multiplier = 1024;
    else if (last_char == 'm' || last_char == 'M')
        multiplier = 1024 * 1024;
    else if (last_char == 'g' || last_char == 'G')
        multiplier = 1024 * 1024 * 1024;

    // std::atoll 会自动忽略字符串末尾的非数字字符（比如 "10m" 会被转成 10）
    long long size = std::atoll(val.c_str()) * multiplier;

    if (loc == NULL) {
        this->_client_max_body_size = size;
    } else {
        loc->client_max_body_size = size;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_methods(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (pos + 1 >= tokens.size() || tokens[pos + 1].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'methods'");
    }

    while (++pos < tokens.size() && tokens[pos].content != ";") {
        if (loc == NULL) {
            this->_methods.push_back(tokens[pos].content);
        } else {
            loc->methods.push_back(tokens[pos].content);
        }
    }

    Utils::expect_semicolon(tokens, pos);
}

void ServerConfig::_handle_return(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing status code for 'return'");
    }

    int code = std::atoi(tokens[pos].content.c_str());

    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing URL for 'return'");
    }

    // 获取 http://...
    std::string url = tokens[pos].content;

    if (loc == NULL) {
        this->_return_code = code;
        this->_return_url  = url;
    } else {
        loc->return_code = code;
        loc->return_url  = url;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_upload_path(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'upload_path'");
    }

    if (loc == NULL) {
        this->_upload_path = tokens[pos].content;
    } else {
        loc->upload_path = tokens[pos].content;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_cgi_path(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'cgi_path'");
    }

    if (loc == NULL) {
        this->_cgi_path = tokens[pos].content;
    } else {
        loc->cgi_path = tokens[pos].content;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_cgi_ext(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'cgi_ext'");
    }

    if (loc == NULL) {
        this->_cgi_ext = tokens[pos].content;
    } else {
        loc->cgi_ext = tokens[pos].content;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::fill_location_defaults(location& loc)
{
    if (loc.root.empty()) loc.root = this->_root;
    if (loc.methods.empty()) loc.methods.push_back("GET");  // 默认允许 GET
    // TODO: 其他字段的默认
}

void ServerConfig::add_location(const location& loc)
{ this->locations.push_back(loc); }

void ServerConfig::parse(std::vector<Token>& tokens, size_t& pos)
{
    if (pos < tokens.size() && tokens[pos].content == "server") { pos++; }

    if (pos >= tokens.size() || tokens[pos].content != "{") {
        throw std::runtime_error("Syntax error: Expected '{' to start server block");
    }
    pos++;

    while (pos < tokens.size() && tokens[pos].content != "}") {
        std::string directive = tokens[pos].content;

        std::cout << "[DEBUG] pos: " << pos << " | directive: [" << directive << "]" << std::endl;
        if (directive == "location") {
            this->parseLocation(tokens, pos);
        } else if (this->hasHandler(directive)) {
            this->executeHandler(directive, tokens, pos, NULL);
        } else {
            throw std::runtime_error("Syntax error: Unknown server directive '" + directive + "'");
        }
    }

    if (pos >= tokens.size() || tokens[pos].content != "}") {
        throw std::runtime_error("Syntax error: Unexpected EOF, missing '}' to close server block");
    }
    pos++;
}

void ServerConfig::parseLocation(std::vector<Token>& tokens, size_t& pos)
{
    location new_loc;

    new_loc.client_max_body_size = 0;
    new_loc.autoindex            = false;

    pos++;
    if (pos >= tokens.size() || tokens[pos].content == "{") {
        throw std::runtime_error("Syntax error: Missing path prefix for location");
    }
    new_loc._prefix = tokens[pos].content;
    pos++;

    if (pos >= tokens.size() || tokens[pos].content != "{") {
        throw std::runtime_error("Syntax error: Expected '{' after location prefix '" +
                                 new_loc._prefix + "'");
    }
    pos++;

    while (pos < tokens.size() && tokens[pos].content != "}") {
        std::string directive = tokens[pos].content;

        if (this->hasHandler(directive)) {
            this->executeHandler(directive, tokens, pos, &new_loc);
        } else {
            throw std::runtime_error("Syntax error: Unknown directive '" + directive +
                                     "' in location '" + new_loc._prefix + "'");
        }
    }

    if (pos >= tokens.size() || tokens[pos].content != "}") {
        throw std::runtime_error("Syntax error: Unexpected EOF, missing '}' for location '" +
                                 new_loc._prefix + "'");
    }
    pos++;

    this->fill_location_defaults(new_loc);
    this->locations.push_back(new_loc);
}

// 用于测试
void ServerConfig::print() const
{
    std::cout << "  [Server Block]" << std::endl;
    std::cout << "    Listen Port: " << this->_listen_port << std::endl;
    std::cout << "    Root: " << this->_root << std::endl;

    std::cout << "    Server Names: ";
    for (size_t i = 0; i < this->_server_names.size(); ++i) {
        std::cout << this->_server_names[i] << " ";
    }
    std::cout << std::endl;

    // 打印 Locations
    for (size_t i = 0; i < this->locations.size(); ++i) {
        const location& loc = this->locations[i];
        std::cout << "    -> [Location: " << loc._prefix << "]" << std::endl;
        std::cout << "       Root: " << loc.root << std::endl;
        std::cout << "       Autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;

        std::cout << "       Methods: ";
        for (size_t j = 0; j < loc.methods.size(); ++j) { std::cout << loc.methods[j] << " "; }
        std::cout << std::endl;
    }
}