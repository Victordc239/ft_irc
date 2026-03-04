/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:53:55 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/04 17:39:00 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 15:51:36 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/03 16:29:45 by vdiez-cu         ###   ########.fr       */
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

extern volatile sig_atomic_t g_running;

class Server
{
	private:
		int _server_fd;
		std::vector<struct pollfd> _fds; // Estructura poll
		std::map<int, Client> _clients;
		std::string _serverPassword;
		static const size_t BUF_SIZE = 512;
		char _buf[512];

		bool nick_in_use(const std::string &nick) const;
		void send_numeric(int fd, const std::string &msg);

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