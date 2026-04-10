NAME        = webserv
CXX         = c++
CXXFLAGS    = -Wall -Werror -Wextra -std=c++98 -I./inc -MMD -MP
RM          = rm -rf

SRCS        = src/main.cpp
OBJ_DIR     = obj
OBJS        = $(SRCS:src/%.cpp=$(OBJ_DIR)/%.o)
DEPS        = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
\t$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: src/%.cpp
\tmkdir -p $(dir $@)
\t$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
\t$(RM) $(OBJ_DIR)

fclean: clean
\t$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
