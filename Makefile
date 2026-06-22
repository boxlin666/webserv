NAME        = webserv
CXX         = c++
CXXFLAGS    = -Wall -Werror -Wextra -std=c++98 -I./inc -MMD -MP
RM          = rm -rf

SRCS        = src/main.cpp \
              src/cgi/CGIHandler.cpp \
              src/core/Cluster.cpp \
              src/core/ConfigParser.cpp \
              src/core/ServerConfig.cpp \
              src/http/HttpRequest.cpp \
              src/http/HttpResponse.cpp \
              src/http/HttpResponseCGI.cpp \
              src/http/HttpResponseGenerator.cpp \
              src/http/HttpResponseGet.cpp \
              src/http/HttpResponseInit.cpp \
              src/http/HttpResponseMethod.cpp \
              src/http/HttpResponseSet.cpp \
              src/http/RequestHandler.cpp \
              src/http/Router.cpp \
              src/network/Connection.cpp \
              src/network/PassiveSocket.cpp \
              src/signal/NotificationPipe.cpp \
              src/utils/Utils.cpp \
              src/utils/debug_envp.cpp \
              src/utils/debug_request_msg_print.cpp

OBJ_DIR     = obj
OBJS        = $(SRCS:src/%.cpp=$(OBJ_DIR)/%.o)
DEPS        = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

PURE_OBJS = $(filter-out $(OBJ_DIR)/main.o, $(OBJS))

-include $(DEPS)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re:
	$(MAKE) fclean
	$(MAKE) all

.PHONY: all clean fclean re
