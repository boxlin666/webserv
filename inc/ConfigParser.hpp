#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ServerConfig.hpp"

struct location;

class ServerConfig;

struct Token {
    std::string content;
    int         line;

    Token(std::string c, int l) : content(c), line(l) {}
};

class ConfigParser {
   private:
    std::vector<ServerConfig*> _servers;

    ConfigParser(const ConfigParser& other);
    ConfigParser& operator=(const ConfigParser& other);

    void tokenize(const std::string& raw_data, std::vector<Token>& tokens);

    // 递归解析函数：使用引用传递 pos，确保全局进度同步
    void parseServer(std::vector<Token>& tokens, size_t& pos);

    // TODO: 校验
    void validateDirectives(const std::vector<ServerConfig>& configs);
    void flush_token(std::string& current, int line, std::vector<Token>& tokens);

   public:
    ConfigParser();
    ~ConfigParser(void);
    // tmp for test
    std::string read_file(const std::string& filepath);
    void        build_config_map(const std::string& config_path);  // => 总入口 parser
    void        print() const;
};

#endif