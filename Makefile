NAME        = webserv
CXX         = c++
CXXFLAGS    = -Wall -Werror -Wextra -std=c++98 -I./inc -MMD -MP -g3
RM          = rm -rf

SRCS        = $(shell find src -name "*.cpp")
OBJ_DIR     = obj
OBJS        = $(SRCS:src/%.cpp=$(OBJ_DIR)/%.o)
DEPS        = $(OBJS:.o=.d)

REQ_TEST = test_request
RES_TEST = test_response

REQ_TEST_SRC = test/request_main.cpp
RES_TEST_SRC = test/response_main.cpp

REQ_TEST_OBJ = $(patsubst test/%.cpp, $(OBJ_DIR)/test/%.o, $(REQ_TEST_SRC))
RES_TEST_OBJ = $(patsubst test/%.cpp, $(OBJ_DIR)/test/%.o, $(RES_TEST_SRC))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/test/%.o: test/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


PURE_OBJS = $(filter-out $(OBJ_DIR)/main.o, $(OBJS))

$(REQ_TEST): $(REQ_TEST_OBJ) $(PURE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ 

$(RES_TEST): $(RES_TEST_OBJ) $(PURE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

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

#curl -v -X POST http://localhost:8080/uploads/post_test --data-binary "@/home/yanzhao/post_test"