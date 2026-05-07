#include "ConfigParser.hpp"

ConfigParser::ConfigParser()
{

}

ConfigParser::~ConfigParser(void)
{

}

void ConfigParser::build_config_map(const std::string& config_path)
{
    std::string raw = read_file(config_path);
    
    std::vector<Token> tokens;

    tokenize(raw, tokens);
    size_t pos = 0;

    while (pos < tokens.size())
    {
        if (tokens[pos].content == "server") {
            // 发现 server 块，开始解析
            // 此时 pos 指向 "server"，parseServer 内部会处理后续的 '{'
            parseServer(tokens, pos); 
        } else {
            throw std::runtime_error("Unknown directive outside server block: [" + tokens[pos].content + "]");
        }
    }
}

std::string ConfigParser::read_file(const std::string& filepath)
{
    struct stat info;
    if (stat(filepath.c_str(), &info) != 0)
    {
        throw std::runtime_error("Config Error: " + std::string(strerror(errno)));
    }
    if (S_ISDIR(info.st_mode)) {
        throw std::runtime_error("Config Error: '" + filepath + "' is a directory");
    }
    std::ifstream file(filepath.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Config Error: Failed to open " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string content = buffer.str();
    if (content.empty()) {
        throw std::runtime_error("Config Error: Configuration file is empty");
    }
    return content;
}

// TODO: 处理root "/var/www/html"
void ConfigParser::tokenize(const std::string& raw_data, std::vector<Token>& tokens)
{
    int         line = 1;
    std::string current;

    for (size_t i = 0; i < raw_data.length(); ++i) {
        char c = raw_data[i];

        // 处理换行：增加行号
        if (c == '\n') {
            flush_token(current, line, tokens);
            line++;
            continue;
        }
        // 处理注释：跳过直到行尾
        if (c == '#') {
            flush_token(current, line, tokens);
            while (i < raw_data.length() && raw_data[i] != '\n') {
                i++;
            }
            // 此时 raw_data[i] 是 \n，交给下一次循环逻辑处理换行
            i--; 
            continue;
        }
        // 处理分隔符
        if (c == '{' || c == '}' || c == ';') {
            flush_token(current, line, tokens);
            std::string special(1, c);
            tokens.push_back(Token(special, line));
            continue;
        }
        // 处理空白符：结束当前的单词
        if (std::isspace(c)) {
            flush_token(current, line, tokens);
            continue;
        }
        // 普通字符累加
        current += c;
    }
    // 处理文件末尾可能留下的单词
    flush_token(current, line, tokens);
}

void ConfigParser::flush_token(std::string& current, int line, std::vector<Token>& tokens) {
    if (!current.empty()) {
        tokens.push_back(Token(current, line));
        current.clear();
    }
}

void ConfigParser::parseServer(std::vector<Token>& tokens, size_t& pos)
{
    ServerConfig* new_server = new ServerConfig();

    try {
        new_server->parse(tokens, pos);
        _servers.push_back(new_server);

    } catch (const std::exception& e) {
        delete new_server;
        throw; 
    }
}

const std::vector<ServerConfig*>& ConfigParser::get_servers(void)const
{
    return (this->_servers);
}

void ConfigParser::print() const {
    std::cout << "=== Configuration Dump ===" << std::endl;
    std::cout << "Total Servers: " << _servers.size() << std::endl;
    for (size_t i = 0; i < _servers.size(); ++i) {
        std::cout << "\n--- Server " << i + 1 << " ---" << std::endl;
        _servers[i]->print();
    }
    std::cout << "==========================" << std::endl;
}