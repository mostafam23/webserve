CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS = main.cpp \
       parsing_validation/ConfigParser.cpp \
	   parsing_validation/ConfigParser_Utils.cpp \
       parsing_validation/ConfigValidator.cpp \
       parsing_validation/ConfigValidator_Impl.cpp \
       logging/Logger.cpp \
       signals/SignalHandler.cpp \
       client_services/ClientRegistry.cpp \
       http/HttpUtils.cpp \
       utils/Utils.cpp \
       server/ServerMain.cpp \
       server/CgiHandler.cpp \
       app/App.cpp \
       app/ArgValidation.cpp

OBJS = $(SRCS:.cpp=.o)

NAME = webserv

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re