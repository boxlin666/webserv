#!/usr/bin/fish

# 42 Webserv Project Initializer

set PROJECT_NAME "webserv"

echo "🚀 Starting to build $PROJECT_NAME structure..."

# 1. 创建目录结构
mkdir -p src/core src/parser src/http src/cgi
mkdir -p inc
mkdir -p conf
mkdir -p www/uploads
mkdir -p obj

echo "📂 Directories created."

# 2. 生成基础的 .gitignore
echo "*.o
*.d
$PROJECT_NAME
.DS_Store
obj/
www/uploads/*
!www/uploads/.gitkeep" > .gitignore

touch www/uploads/.gitkeep
echo "🙈 .gitignore configured."

# 3. 生成基础配置文件
echo "server {
    listen 8080;
    server_name localhost;
    root ./www;
    index index.html;

    location / {
        methods GET POST;
    }

    location /cgi-bin {
        methods GET POST;
        cgi_pass ./cgi-bin/php-cgi;
    }
}" > conf/default.conf
echo "📝 Default config created in conf/."

# 4. 生成基础 HTML
echo "<html><body><h1>Hello Webserv!</h1></body></html>" > www/index.html

# 5. 生成符合 42 规范的 Makefile (支持自动依赖)
echo "NAME        = $PROJECT_NAME
CXX         = c++
CXXFLAGS    = -Wall -Werror -Wextra -std=c++98 -I./inc -MMD -MP
RM          = rm -rf

SRCS        = src/main.cpp
OBJ_DIR     = obj
OBJS        = \$(SRCS:src/%.cpp=\$(OBJ_DIR)/%.o)
DEPS        = \$(OBJS:.o=.d)

all: \$(NAME)

\$(NAME): \$(OBJS)
\t\$(CXX) \$(CXXFLAGS) \$(OBJS) -o \$(NAME)

\$(OBJ_DIR)/%.o: src/%.cpp
\tmkdir -p \$(dir \$@)
\t\$(CXX) \$(CXXFLAGS) -c \$< -o \$@

-include \$(DEPS)

clean:
\t\$(RM) \$(OBJ_DIR)

fclean: clean
\t\$(RM) \$(NAME)

re: fclean all

.PHONY: all clean fclean re" > Makefile
echo "🛠️  Makefile generated (with C++98 and dependency tracking)."

# 6. 生成 main.cpp 占位符
echo "#include <iostream>

int main(int argc, char **argv) {
    if (argc > 2) {
        std::cerr << \"Usage: ./webserv [config_file]\" << std::endl;
        return 1;
    }
    std::cout << \"Starting $PROJECT_NAME...\" << std::endl;
    return 0;
}" > src/main.cpp

echo "✅ Project '$PROJECT_NAME' is ready to go!"