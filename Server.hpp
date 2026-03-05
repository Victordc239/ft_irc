/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:53:55 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/05 11:06:35 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include <netinet/in.h>
#include <csignal>
#include "Client.hpp"
#include "Channel.hpp"

extern volatile sig_atomic_t g_running;

class Server
{
	private:
		int _server_fd;
		std::vector<struct pollfd> _fds; // Estructura poll
		std::string _serverPassword;
		static const size_t BUF_SIZE = 512;
		char _buf[512];
		std::map<int, Client> _clients;
		std::map<std::string, Channel> _channels;

		bool nick_in_use(const std::string &nick) const;
		void send_numeric(int fd, const std::string &msg);
		bool	handle_initial_authentication(size_t &i, const std::string &line);
		void	handle_nick_command(int clientFd, const std::string &line);
		void	handle_user_command(int clientFd, const std::string &line);

	public:
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);
		~Server();

		bool init_and_listen(long port, const std::string &password);
		int set_nonblock(int fd);
		int run_loop();
};

#endif