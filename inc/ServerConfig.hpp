#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include "ConfigParser.hpp"
#include <vector>
#include <iterator>

class ServerConfig
{
    private:
        ServerConfig(const ServerConfig& other);
        ServerConfig& operator=(const ServerConfig& other);

        std::string _server_name;
        int _listen_port;
        std::string _host;

        std::string _root;
        std::string _index;
        size_t  _client_max_body_size;
        std::map<int, std::string> error_page;

        struct location //暂时不考虑变成类，只作为结构体，后期如有需求再做调整
        {
            //static content
            std::string _prefix;
            std::string _path;
            std::string _upload_path;
            std::string root;
            std::string index_file;
            bool autoindex;

            //access
            std::vector<string> methods;
            std::size_t client_max_body_size;  
            std::map<int, std::string> _redirect; //跳转路径
       
            std::map<std::string, std::string> _cgi_param; //cgi 扩展配置
        };

        std::vector<Token>::const_iterator begin; //从构造函数中传递而来
        std::vector<Token>::const_iterator end; //同上
        std::vector<location> locations; //最长前缀匹配 不需要使用map,需要循环遍历vector得到最长匹配（我是说run connection阶段）

    public:
        ServerConfig(std::vector<Token>::const_iterator begin, std::vector<Token>::const_iterator end); //截获ConfigParser 内部相关server的开启单词和结束单词
        ~ServerConfig(void);
        bool parse(void); //开始正式解析，填充 std::vector<location> locations;
        //location get_location(void)const;

};
#endif