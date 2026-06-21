#include "Utils.hpp"

void printEnvMap(const std::map<std::string, std::string>& env_map)
{
    std::cout << "\033[34m" << "---------- [DEBUG ENV MAP] ----------" << "\033[0m" << std::endl;

    typedef std::map<std::string, std::string>::const_iterator const_it;

    for (const_it it = env_map.begin(); it != env_map.end(); ++it) {
        std::cout << it->first << " -> [" << it->second << "]" << std::endl;
    }

    std::cout << "\033[34m" << "-------------------------------------" << "\033[0m" << std::endl;
}
