#include "CGIHandler.hpp"

CGIHandler::CGIHandler()
{
}

CGIHandler::~CGIHandler()
{
}

void CGIHandler::_mapToEnvp() {
    _clearEnvp();
    _envp = new char*[_envMap.size() + 1];

    int i = 0;
    for (std::map<std::string, std::string>::iterator it = _envMap.begin(); 
         it != _envMap.end(); ++it) {
        std::string entry = it->first + "=" + it->second;
        _envp[i] = strdup(entry.c_str());
        i++;
    }
    _envp[i] = NULL;
}
void CGIHandler::_clearEnvp() {
    if (_envp) {
        for (int i = 0; _envp[i] != NULL; i++) {
            free(_envp[i]);
        }
        delete[] _envp;
        _envp = NULL;
    }
}

std::string CGIHandler::getRawResponse()
{
    std::string res;

    return res;
}

