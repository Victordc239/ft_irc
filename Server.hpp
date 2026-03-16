/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:53:55 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/16 23:03:22 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <map>
#include <poll.h>
#include <netinet/in.h>
#include <csignal>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <sys/socket.h>
#include <cstdlib>
#include <climits>
#include "Client.hpp"
#include "Channel.hpp"
#include "FileTransfer.hpp"
#include "Bot.hpp"

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
		std::map<unsigned long, FileTransfer> _transfers;
		unsigned long _nextTransferId;
		std::map<int, unsigned long> _fdToTransferId; // fd -> transfer id
		Bot _bot;

		bool	nickInUse(const std::string &nick) const;
		void	sendNumeric(int fd, const std::string &msg);
		int	getFdByNick(const std::string &nick) const;
		bool	handleInitialAuthentication(size_t &i, const std::string &line);
		void	handleNickCommand(int clientFd, const std::string &line);
		void	handleUserCommand(int clientFd, const std::string &line);
		void	handleJoinCommand(int clientFd, const std::string &line);
		void	handlePartCommand(int clientFd, const std::string &line);
		void	handlePrivmsgCommand(int clientFd, const std::string &line);
		bool	handleDccSendInPrivmsg(int clientFd, int dst_fd, const std::string &text, const std::string &prefix, const std::string &target);
		void	handleKickCommand(int clientFd, const std::string &line);
		void	handleInviteCommand(int clientFd, const std::string &line);
		void	handleTopicCommand(int clientFd, const std::string &line);
		void	handleModeCommand(int clientFd, const std::string &line);
		void	mode_i(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &prefix);
		void	mode_t(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &prefix);
		void	mode_k(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &param, const std::string &prefix);
		void	mode_o(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &targetNick, const std::string &prefix);
		void	mode_l(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &param, const std::string &prefix);
		void	mode_user(int clientFd, const std::string &target, const std::string &rest);
		bool	handleFileTransferEvent(size_t &i);
		bool	flushBufferToFd(std::vector<struct pollfd> &fds, FileTransfer &ft, std::string &buffer, int dst, bool countBytes);
		void	setPollEvents(std::vector<struct pollfd> &fds, int fd, short events);
		void	removePollFd(std::vector<struct pollfd> &fds, int fd);
		void	cleanupTransfersForClient(int badfd);

	public:
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);
		~Server();

		bool	initAndListen(long port, const std::string &password);
		int	setNonblock(int fd);
		int	runLoop();

};

std::string	intToString(int n);
long	ftStrtol(const char *str, char **endptr);
bool	parseDccIpToken(const std::string &tok, struct in_addr &out);


#endif