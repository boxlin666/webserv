#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
    : _server_names(),
      _listen_addrs(),
      _index(),
      _root(""),
      _client_max_body_size(1048576), // 默认 1MB 
      _error_pages(),
      _methods(),
      _return_code(0),
      _return_url(""),
      _upload_path(""),
      _cgi_path(""),
      _cgi_ext(""),
      _cgi_script(""),
      _autoindex(false),
      _handler_map(),
      locations()
{
    this->_init_handlers();
}


ServerConfig::~ServerConfig() {}

int ServerConfig::string_to_port(const std::string& port_str)const
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
    return (static_cast<int>(val));
}

void    ServerConfig::set_listen_addrs(const std::string& listen_data)
{
    std::string host;
    int port;

    std::size_t colon_pos = listen_data.find(':');
    if (colon_pos == std::string::npos)
    {
        port = this->string_to_port(listen_data);
        host = "0.0.0.0";
    }
    else
    {
        if (colon_pos + 1 == listen_data.size())
            throw std::runtime_error("Missing port number in listen data: " + listen_data);
        std::string port_str = listen_data.substr(colon_pos + 1);
        port = this->string_to_port(port_str);
        host = listen_data.substr(0, colon_pos);

        if (!validate_listen_fd(host, port_str)) return ;
    }
    this->_listen_addrs.push_back(std::make_pair(host, port));
}

bool    ServerConfig::validate_listen_fd(const std::string& host, const std::string& port_str)const
{
    struct addrinfo hints, *result = NULL;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;

    int status = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if (status != 0)
        throw std::runtime_error("Invalid listen host '" + host + "': " + gai_strerror(status));
    if (result != NULL)
        freeaddrinfo(result);
    return (true);
}

const std::vector<location>& ServerConfig::get_locations(void)const
{
    return (this->locations);
}

void ServerConfig::_init_handlers()
{
    _handler_map["root"]                 = &ServerConfig::_handle_root;
    _handler_map["methods"]              = &ServerConfig::_handle_methods;
    _handler_map["autoindex"]            = &ServerConfig::_handle_autoindex;
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
    _handler_map["cgi_script"]           = &ServerConfig::_handle_cgi_script;
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

    struct stat info;
    if (stat(path.c_str(), &info) != 0)
           throw std::runtime_error("Configuration error: 'root' path \"" + path + "\" does not exist!");
    else if (!S_ISDIR(info.st_mode))
            throw std::runtime_error("Configuration error: 'root' path \"" + path + "\" is a file, but a directory is required!");

    if (access(path.c_str(), R_OK | X_OK) != 0) 
        throw std::runtime_error("Configuration error: 'root' path \"" + path +
            "\" exists, but server process has NO permission to access it (Permission denied)!");
    
    if (loc == NULL) {
        // 当前在解析 server 块，存给 ServerConfig 自己的成员变量
        if (!this->_root.empty())
            throw std::runtime_error("Configuration error: Multiple 'root' path found in server block");
        this->_root = path;
    } else {
        // 当前在解析 location 块，存给 location 结构体
        if (!loc->root.empty())
            throw std::runtime_error("Configuration error: Multiple 'root' path found in location block");
        loc->root = path;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_autoindex(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size()) throw std::runtime_error("Missing value for autoindex");

    bool is_on = false;

    if (tokens[pos].content == "on")
        is_on = true;
    else if (tokens[pos].content == "off")
        is_on = false;
    else 
        throw std::runtime_error("autoindex value '" + tokens[pos].content + "' is invalid");

    if (loc == NULL)
         this->_autoindex = is_on;
    else
    {
        loc->autoindex = is_on;
        loc->autoindex_set = true;
    }
    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_listen(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (loc != NULL) 
        throw std::runtime_error(
            "Syntax error: 'listen' directive is not allowed in location block");
    
    if (++pos >= tokens.size()) 
        throw std::runtime_error("Syntax error: Missing port number for 'listen'");
    

    this->set_listen_addrs(tokens[pos].content.c_str());

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_server_name(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    int current_line = 0;

    if (loc != NULL) {
        throw std::runtime_error(
            "Syntax error: 'server_name' directive is not allowed in location block");
    }
    
    current_line = tokens[pos].line;

    if (pos + 1 >= tokens.size() || tokens[pos + 1].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'server_name'");
    }

    // iterate to read multiple server name (ex server_name a.com b.com;)
    while (++pos < tokens.size() && tokens[pos].content != ";" && tokens[pos].line == current_line) 
    {
	if (!_is_valid_server_name(tokens[pos].content))
		throw std::runtime_error("Syntax error: server name value '" + tokens[pos].content + "' is invalid");
        this->_server_names.push_back(tokens[pos].content);
    }

    if (this->_server_names.size() == 0)
        throw std::runtime_error("Syntax error: Missing server_name value after server_name directive");
    Utils::expect_semicolon(tokens, pos);
}

bool ServerConfig::_is_valid_server_name(const std::string& server_name) const 
{
	if (server_name.empty())
		return (false);

	for (std::size_t i = 0; i < server_name.length(); ++i) 
	{
		char c = server_name[i];
		if (std::iscntrl(c) || c <= 32 || c == 127)
	    		return (false);
	
		if (c == '/' || c == '?' || c == '#') 
	    		return (false);
	
		if (c == '\\' || c == '*' || c == '^' || c == '`') 
	    		return (false);
	}
	return (true);
}

void ServerConfig::_handle_index(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    int current_line = 0;

    if (pos + 1 >= tokens.size() || tokens[pos + 1].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'index'");
    }

    current_line  = tokens[pos].line;

    while (++pos < tokens.size() && tokens[pos].content != ";" && tokens[pos].line == current_line) {
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
        if (tokens[pos].content.length() == 3 && Utils::is_digit_str(tokens[pos].content))
        {
            int error_code = Utils::convertThreeDigit(tokens[pos].content);

            if (error_code >= 400 && error_code <= 599)
                codes.push_back(error_code);
            else
                throw std::runtime_error("Syntax error: Invalid status code " + tokens[pos].content  + " for 'error_page'");
        }
        else
            throw std::runtime_error("Syntax error: Invalid status code " + tokens[pos].content  + " for 'error_page'");
        pos++;
    }

    if (pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing file path for 'error_page'");
    }
    if (codes.empty()) {
        throw std::runtime_error("Syntax error: Missing status code(s) for 'error_page'");
    }

    std::string error_file_path = tokens[pos].content;

    if (!error_file_path.empty() && error_file_path[0] != '/')
        throw std::runtime_error("Syntax error: Invalid error page path: " + error_file_path);

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
    std::string num_val    = val;
    long long   multiplier = 1;
    char        last_char  = val[val.length() - 1];

    if (last_char == 'k' || last_char == 'K')
        multiplier = 1024;
    else if (last_char == 'm' || last_char == 'M')
        multiplier = 1024 * 1024;
    else if (last_char == 'g' || last_char == 'G')
        multiplier = 1024 * 1024 * 1024;

    if (std::isalpha(last_char) && multiplier >= 1024)
        num_val = val.substr(0, val.length() - 1);

    if (num_val.empty() || !Utils::is_digit_str(num_val))
        throw std::runtime_error("Syntax error: Invalid client_max_body_size value '" + val + "'");

    long long parsed_num = 0;
    std::stringstream ss(num_val);

    if (!(ss >> parsed_num) || !ss.eof())
        throw std::runtime_error("Configuration error: 'client_max_body_size' value '" + val + "' is overflowed");

    if (parsed_num > LLONG_MAX / multiplier ||(parsed_num * multiplier) > 1099511627776LL)
        throw std::runtime_error("Configuration error: 'client_max_body_size' value '" + val +  "' is overflowed");

    long long size = parsed_num * multiplier;

    if (size < 0 || (multiplier > 1 && size < parsed_num))
        throw std::runtime_error("Configuration error: 'client_max_body_size' value '" + val + "' is overflowed");
 
    if (loc == NULL) {
        this->_client_max_body_size = size;
    } else {
        loc->client_max_body_size_set = true;
        loc->client_max_body_size = size;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_methods(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    int current_line = 0;

    if (pos + 1 >= tokens.size() || tokens[pos + 1].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'methods'");
    }

    current_line = tokens[pos].line;

    while (++pos < tokens.size() && tokens[pos].content != ";" && tokens[pos].line == current_line) {
        if (tokens[pos].content != "GET" && tokens[pos].content != "POST" && 
            tokens[pos].content != "HEAD" && tokens[pos].content != "DELETE")
            throw std::runtime_error("Configuration error: method " + tokens[pos].content + " is not allowed.");
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
    if (++pos >= tokens.size() || tokens[pos].content == ";")
        throw std::runtime_error("Syntax error: Missing status code for 'return'");

    int code = Utils::convertThreeDigit(tokens[pos].content);

    if (code < 300 || code > 308 || code == 304)
        throw std::runtime_error("Syntax error: Invalid status code " + tokens[pos].content  + " for 'return status code'");

    if (++pos >= tokens.size() || tokens[pos].content == ";")
        throw std::runtime_error("Syntax error: Missing URL for 'return'");

    // fetch the  http://...
    std::string url = tokens[pos].content;

    if (url.empty())
        throw std::runtime_error("Return URL cannot be empty");

    for (size_t i = 0; i < url.length(); ++i) 
    {
        if (std::isspace(url[i]))
            throw std::runtime_error("Return URL contains invalid whitespace characters");
    }

    bool is_absolute = (url.compare(0, 7, "http://") == 0 || 
                        url.compare(0, 8, "https://") == 0);
    bool is_relative = (url[0] == '/');

    if (!is_absolute && !is_relative) 
        throw std::runtime_error("Invalid return URL format: must start with 'http://', 'https://' or '/'");

    if (loc == NULL) 
    {
        this->_return_code = code;
        this->_return_url  = url;
    } 
    else 
    {
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

    std::string path = tokens[pos].content;

    struct stat info;
    if (stat(path.c_str(), &info) != 0)
           throw std::runtime_error("Configuration error: 'upload_path' \"" + path + "\" does not exist!");
    else if (!S_ISDIR(info.st_mode))
            throw std::runtime_error("Configuration error: 'upload_path' \"" + path + "\" is a file, but a directory is required!");

    if (access(path.c_str(), R_OK | W_OK | X_OK) != 0) 
        throw std::runtime_error("Configuration error: 'upload_path' \"" + path +
            "\" exists, but server process has NO permission to access it (Permission denied)!");

    if (loc == NULL) 
    {
        if (!this->_upload_path.empty())
            throw std::runtime_error("Configuration error: Multiple 'upload path' found in server block");
        this->_upload_path = tokens[pos].content;
    }
    else 
    {
        if (!loc->upload_path.empty())
            throw std::runtime_error("Configuration error: Multiple 'upload path' found in location block");
        loc->upload_path = tokens[pos].content;
        loc->root = tokens[pos].content;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_handle_cgi_path(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'cgi_path'");
    }

    std::string path = tokens[pos].content;

    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        throw std::runtime_error("Configuration error: 'cgi_path' \"" + path + "\" does not exist!");
    else if (!S_ISREG(info.st_mode)) 
           throw std::runtime_error("Configuration error: 'cgi_path' \"" + path + "\" is a directory, but an executable file is required!");
      
    if (access(path.c_str(), X_OK) != 0)
        throw std::runtime_error("Configuration error: 'cgi_path' \"" + path +
            "\" exists, but server process has NO executable permission (X_OK Denied)!"); 

    if (loc == NULL) 
    {
        if (!this->_cgi_path.empty())
            throw std::runtime_error("Configuration error: Multiple 'cgi_path' found in server block");
        this->_cgi_path = tokens[pos].content;
    } 
    else 
    {
        if (!loc->cgi_path.empty())
            throw std::runtime_error("Configuration error: Multiple 'cgi_path' found in location block");
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

void ServerConfig::_handle_cgi_script(std::vector<Token>& tokens, size_t& pos, location* loc)
{
    if (++pos >= tokens.size() || tokens[pos].content == ";") {
        throw std::runtime_error("Syntax error: Missing value for 'cgi_script'");
    }

    if (loc == NULL) {
        this->_cgi_script = tokens[pos].content;
    } else {
        loc->cgi_script = tokens[pos].content;
    }

    Utils::expect_semicolon(tokens, ++pos);
}

void ServerConfig::_fill_location_defaults(location& loc)
{
    if (loc.root.empty())
        loc.root = this->_root;

    if (loc.methods.empty()) 
    {
        if (!this->_methods.empty()) 
            loc.methods = this->_methods;
        else
            loc.methods.push_back("GET");
    }

    if (loc.index.empty()) 
    {
        if (!this->_index.empty()) 
            loc.index = this->_index;
    }

    if (loc.autoindex_set == false) 
    {
        loc.autoindex = this->_autoindex; 
        loc.autoindex_set = true; 
    }

    if (loc.client_max_body_size == 1048576  && loc.client_max_body_size_set == false &&
        this->_client_max_body_size != 1048576)
        loc.client_max_body_size = this->_client_max_body_size;

    for (std::map<int, std::string>::const_iterator it = this->_error_pages.begin(); 
         it != this->_error_pages.end(); ++it) 
    {
        loc.error_pages.insert(*it);
    }
 
    if (loc.cgi_path.empty())   loc.cgi_path = this->_cgi_path;
    if (loc.cgi_ext.empty())    loc.cgi_ext = this->_cgi_ext;
    if (loc.cgi_script.empty()) loc.cgi_script = this->_cgi_script;

    if (loc.upload_path.empty()) 
        loc.upload_path = this->_upload_path;
     
    if (loc.return_code == 0 && this->_return_code != 0) 
    {
        loc.return_code = this->_return_code;
        loc.return_url = this->_return_url;
    }    
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

    for (size_t i = 0; i < this->locations.size(); ++i) 
        this->_fill_location_defaults(this->locations[i]);
    
    for (size_t i = 0; i < this->locations.size(); ++i) { 
        this->_validate_location(this->locations[i]);

        for (size_t j = i + 1; j < this->locations.size(); ++j) {
            if (this->locations[i]._prefix == this->locations[j]._prefix) {
                throw std::runtime_error("Configuration error: duplicate location prefix \"" 
                                        + this->locations[i]._prefix + "\" in server block");
            }
        }
    }
    pos++;
}

void ServerConfig::_validate_location(const location& loc) const
{
    if ((loc.cgi_path.empty() && !loc.cgi_ext.empty()) || 
        (!loc.cgi_path.empty() && loc.cgi_ext.empty())) {
        throw std::runtime_error("Configuration error: missing cgi_path or cgi_ext for location prefix \"" + loc._prefix + "\"");
    }
    
    if (!loc.cgi_path.empty() && !loc.cgi_ext.empty())
    {
        if (loc.cgi_ext == ".py" && loc.cgi_path.find("python") == std::string::npos) 
            throw std::runtime_error("Config Error: .py extension must match a python CGI path for prefix \"" + loc._prefix + "\"");

        if (loc.cgi_ext == ".php" && loc.cgi_path.find("php") == std::string::npos) 
            throw std::runtime_error("Config Error: .php extension must match a php CGI path for prefix \"" + loc._prefix + "\"");
    }
}

void ServerConfig::parseLocation(std::vector<Token>& tokens, size_t& pos)
{
    location new_loc;

    //new_loc.client_max_body_size = 0;
    //new_loc.autoindex            = false;

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
    this->locations.push_back(new_loc);
}

// 用于测试
void ServerConfig::print() const
{
    std::cout << "  [Server Block]" << std::endl;

    /*for (size_t i = 0; i < this->_listen_ports.size(); ++i) 
    {
        std::cout << "listen_port " << i << " " <<  this->_listen_ports[i] << std::endl;
    }*/

    for (size_t i = 0; i < this->_listen_addrs.size(); ++i) 
    {
        std::cout << i << ": Host = " << this->_listen_addrs[i].first << "; Port number = " << this->_listen_addrs[i].second << std::endl;
    }

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

const std::string &ServerConfig::get_error_page(int status_code) const
{
    std::map<int, std::string>::const_iterator it = _error_pages.find(status_code);
    if (it != _error_pages.end())
    {
        return it->second;
    }
    static const std::string empty = "";
    return empty;
}
