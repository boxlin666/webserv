NAME        = webserv
CXX         = c++
CXXFLAGS    = -Wall -Werror -Wextra -std=c++98 -I./inc -MMD -MP -g3
RM          = rm -rf

# --- 核心路径定义 ---
SRCS        = $(shell find src -name "*.cpp")
OBJ_DIR     = obj
OBJS        = $(SRCS:src/%.cpp=$(OBJ_DIR)/%.o)
DEPS        = $(OBJS:.o=.d)

# --- 测试程序路径定义 (去掉了多余的 ./ ) ---
REQ_TEST = test_request
RES_TEST = test_response

REQ_TEST_SRC = test/request_main.cpp
RES_TEST_SRC = test/response_main.cpp

# 确保测试用的 .o 放在 obj/test 目录下
REQ_TEST_OBJ = $(patsubst test/%.cpp, $(OBJ_DIR)/test/%.o, $(REQ_TEST_SRC))
RES_TEST_OBJ = $(patsubst test/%.cpp, $(OBJ_DIR)/test/%.o, $(RES_TEST_SRC))

all: $(NAME)

# --- 编译主程序 ---
$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

# 明确指定 test 的规则
$(OBJ_DIR)/test/%.o: test/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 明确指定 src 的规则
$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


# --- 编译测试程序 (修正了链接命令和依赖) ---

# 注意：这里需要排除掉原本的 src/main.o 以防两个 main 函数冲突
PURE_OBJS = $(filter-out $(OBJ_DIR)/main.o, $(OBJS))

$(REQ_TEST): $(REQ_TEST_OBJ) $(PURE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ 

$(RES_TEST): $(RES_TEST_OBJ) $(PURE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# 专门针对 test 目录的编译规则
$(OBJ_DIR)/test/%.o: test/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)
-include $(REQ_TEST_OBJ:.o=.d)
-include $(RES_TEST_OBJ:.o=.d)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME) $(REQ_TEST) $(RES_TEST)

re: fclean all

.PHONY: all clean fclean re