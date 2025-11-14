CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98


SRCS = main.cpp \
       parsing_validation/ConfigParser.cpp \
       parsing_validation/ConfigParser_Utils.cpp \
       parsing_validation/ConfigParser_Internal.cpp \
       parsing_validation/ConfigValidator.cpp \
       parsing_validation/ConfigValidator_Impl.cpp \
       logging/Logger.cpp \
       signals/SignalHandler.cpp \
       concurrency/ClientRegistry.cpp \
       http/HttpUtils.cpp \
       utils/Utils.cpp \
       server/ClientHandler.cpp \
       server/ServerMain.cpp \
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

functions-all:
	@echo "Listing all function calls in all .cpp files..."
	@for f in $(SRCS); do \
	    echo "---- $$f ----"; \
	    grep -oE '[A-Za-z_][A-Za-z0-9_]*\s*\(' $$f \
	    | sed 's/[( ]//g' \
	    | sort -u; \
	    echo ""; \
	done
# Get all function calls from all SRCS (raw list)
function-calls:
	@for f in $(SRCS); do \
	    grep -oE '[A-Za-z_][A-Za-z0-9_]*\s*\(' $$f \
	    | sed 's/[( ]//g'; \
	done | sort -u > .all_calls.tmp
	@echo "Saved all calls to .all_calls.tmp"

# Get all functions defined in your own code using ctags
function-defined:
	@ctags -x --c++-kinds=f $(SRCS) | awk '{print $$1}' | sort -u > .all_defined.tmp
	@echo "Saved all defined functions to .all_defined.tmp"

# Show predefined functions (library calls)
functions-predefined: function-calls function-defined
	@echo "Predefined (library) functions used in your project:"
	@comm -23 .all_calls.tmp .all_defined.tmp
