# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/12/02 11:18:27 by victor            #+#    #+#              #
#    Updated: 2026/03/23 18:45:00 by sofernan         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME		= ircserv

SRCS		= main.cpp \
			Server.cpp \
			Client.cpp \
			Channel.cpp \
			ServerCommands.cpp \
			ServerOperatorsCommands.cpp \
			ServerCommandsMode.cpp \
			ServerUtils.cpp \
			FileTransfer.cpp \
			ServerFileTransfer.cpp \
			Bot.cpp

HEADERS	= Client.hpp \
			Server.hpp \
			Channel.hpp \
			FileTransfer.hpp \
			Bot.hpp

OBJS		= $(SRCS:.cpp=.o)

CXX		= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98

VALGRIND_PORT	= 1201
VALGRIND_PASSWORD	= hola

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

valgrind: fclean all
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes --trace-children=yes --error-exitcode=1 ./$(NAME) $(VALGRIND_PORT) $(VALGRIND_PASSWORD)

.PHONY: all clean fclean re valgrind