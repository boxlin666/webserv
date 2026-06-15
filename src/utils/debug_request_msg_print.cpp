#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

#define RESET   "\033[0m"
#define RED     "\033[31m"     
#define GREEN   "\033[32m"      
#define YELLOW  "\033[33m"      
#define BLUE    "\033[34m"      
#define PURPLE  "\033[35m"      
#define CYAN    "\033[36m"      
#define WHITE   "\033[37m"      


void debug_msg_print(const std::string& variable, const std::string& content, 
                     const std::string& color, size_t max_len = 200) 
{
    std::cout << "\n" << color << "---start of " << variable << "---" << RESET << std::endl; 

    size_t print_len = std::min(content.size(), max_len); 
    
    for (size_t i = 0; i < print_len; ++i) {
        char ch = content[i];

        if (ch == '\r') 
        {
            std::cout << YELLOW << "\\r" << RESET;
        } 
        else if (ch == '\n') 
        {
            std::cout << YELLOW << "\\n" << RESET << "\n"; 
        } 
        else if (isprint(static_cast<unsigned char>(ch))) {
            std::cout << color << ch << RESET;
        }  
        else  //no printable char is RED
        {
            std::cout << RED << "." << RESET; 
        }
    }

    if (content.size() > max_len) 
    {
        std::cout << WHITE << "\n... [Truncated! Total size: " << content.size() << " bytes]" << RESET;
    }

    std::cout << "\n" << color << "---end of " << variable << "---\n" << RESET << std::endl;
}