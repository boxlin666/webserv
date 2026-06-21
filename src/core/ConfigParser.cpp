#include "ConfigParser.hpp"

ConfigParser::ConfigParser() {}

ConfigParser::~ConfigParser()
{
    std::vector<ServerConfig*>::iterator it;
    for (it = this->_servers.begin(); it != this->_servers.end(); it++) delete *it;
    this->_servers.clear();
}

void ConfigParser::build_config_map(const std::string& config_path)
{
    std::string raw = read_file(config_path);

    std::vector<Token> tokens;

    tokenize(raw, tokens);
    size_t pos = 0;

    while (pos < tokens.size()) {
        if (tokens[pos].content == "server") {
            parseServer(tokens, pos);
        } else {
            throw std::runtime_error("Unknown directive outside server block: [" +
                                     tokens[pos].content + "]");
        }
    }
}

std::string ConfigParser::read_file(const std::string& filepath)
{
    struct stat info;
    if (stat(filepath.c_str(), &info) != 0)
        throw std::runtime_error("Config Error: Unable to access or find the file at '" + filepath +
                                 "'. Check path and permissions.");

    if (S_ISDIR(info.st_mode)) {
        throw std::runtime_error("Config Error: '" + filepath + "' is a directory");
    }
    std::ifstream file(filepath.c_str());
    if (!file.is_open()) { throw std::runtime_error("Config Error: Failed to open " + filepath); }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string content = buffer.str();
    if (content.empty()) { throw std::runtime_error("Config Error: Configuration file is empty"); }
    return content;
}

void ConfigParser::tokenize(const std::string& raw_data, std::vector<Token>& tokens)
{
    int         line = 1;
    std::string current;

    for (size_t i = 0; i < raw_data.length(); ++i) {
        char c = raw_data[i];

        if (c == '\n') {
            flush_token(current, line, tokens);
            line++;
            continue;
        }
        if (c == '#') {
            flush_token(current, line, tokens);
            while (i < raw_data.length() && raw_data[i] != '\n') { i++; }
            i--;
            continue;
        }
        if (c == '{' || c == '}' || c == ';') {
            flush_token(current, line, tokens);
            std::string special(1, c);
            tokens.push_back(Token(special, line));
            continue;
        }
        if (std::isspace(c)) {
            flush_token(current, line, tokens);
            continue;
        }
        current += c;
    }
    flush_token(current, line, tokens);
}

void ConfigParser::flush_token(std::string& current, int line, std::vector<Token>& tokens)
{
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

const std::vector<ServerConfig*>& ConfigParser::get_servers(void) const
{ return (this->_servers); }

void ConfigParser::print() const
{
    std::cout << "=== Configuration Dump ===" << std::endl;
    std::cout << "Total Servers: " << _servers.size() << std::endl;
    for (size_t i = 0; i < _servers.size(); ++i) {
        std::cout << "\n--- Server " << i + 1 << " ---" << std::endl;
        _servers[i]->print();
    }
    std::cout << "==========================" << std::endl;
}
