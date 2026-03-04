# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/12/02 11:18:27 by victor            #+#    #+#              #
#    Updated: 2026/03/04 14:43:53 by vdiez-cu         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME		= ircserv

SRCS		= main.cpp \
			Server.cpp \
			Client.cpp

HEADERS	= Client.hpp \
			Server.hpp

OBJS		= $(SRCS:.cpp=.o)

CXX		= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98

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

.PHONY: all clean fclean re