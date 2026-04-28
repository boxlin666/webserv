#ifndef CONFIG_PARSER_HPP
# define CONFIG_PARSER_HPP

#include "ServerConfig.hpp"
#include <string>
#include <map>

struct Token
{
    std::string value;
    int line_num; //方便调试
};

class ConfigParser
{
    private:
        std::map<int, vector<ServerConfig>> _config_map; // key = Port Number , Value = vector(Server1, Server2, Server3)
  
        ConfigParser(const ConfigParser& other);
        ConfigParser& operator=(const ConfigParser& other);

        //报错使用try catch 所以void也没有关系？
        void pre_process(void); //1:去掉所有备注 , 删除多余空格, 检查大括号是否对称, 是否是空文件, 导入_config_content内部？ 
        void tokenize(void); //2: 把所有单词存入 vector token 内部
        void check_key_word(void); //3:: 检查关键词是否对应 "listen" "server_name" "root" "location"  ...(非法关键词退出)
        void extract_all_ports(void); //4:检查端口合法性和权限 (0 - 65536 之间) 并初步建立map 的keys
        void fill_server_configs(void); //5: 边填写边检查,开始调用 ServConfig类 

        std::string _config_content;

        std::vector<Token> tokens;

    public:
        ConfigParser(char *config_file);
        ~ConfigParser(void);

        void build_config_map(void); // => 总入口 parser
};

#endif