/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:53:55 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/27 16:44:57 by sofernan         ###   ########.fr       */
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
		std::vector<struct pollfd> _fds;
		std::string _serverPassword;
		static const size_t BUF_SIZE = 512;
		char _buf[512];
		std::map<int, Client> _clients;
		std::map<std::string, Channel> _channels;
		std::map<unsigned long, FileTransfer> _transfers;
		unsigned long _nextTransferId;
		std::map<int, unsigned long> _fdToTransferId;
		Bot _bot;

		bool	isNickInUse(const std::string &nick) const;
		void	sendNumericMessage(int fd, const std::string &msg);
		int		findFdByNick(const std::string &nick) const;
		bool	handleAuthenticationCmds(size_t &i, const std::string &line);
		void	handleNickCommand(int clientFd, const std::string &line);
		void	handleUserCommand(int clientFd, const std::string &line);
		void	handleJoinCommand(int clientFd, const std::string &line);
		void	sendJoinInfo(int clientFd, const std::string &nameChannel);;
		void	handlePartCommand(int clientFd, const std::string &line);
		void	handlePrivmsgCommand(int clientFd, const std::string &line);
		void	processPrivmsgCommand(int clientFd, const std::string &target, const std::string &text, const std::string &prefix, const std::string &nick);
		bool	handleDccSend(int clientFd, int dst_fd, const std::string &text, const std::string &prefix, const std::string &target);
		bool	handleDccSendProxy(int clientFd, int dst_fd, const std::vector<std::string> &toks, const std::string &prefix, const std::string &target, const std::string &filename, unsigned long fsize, unsigned long transferId);
		void	handleKickCommand(int clientFd, const std::string &line);
		void	handleInviteCommand(int clientFd, const std::string &line);
		void	handleTopicCommand(int clientFd, const std::string &line);
		void	handleChannelModes(int clientFd, const std::string &line);
		void	handleModeChange(int clientFd, Channel &channel, const std::string &channelName, const std::string &rest, const std::string &nick, const std::string &prefix);
		void	mode_i(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &prefix);
		void	mode_t(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &prefix);
		void	mode_k(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &param, const std::string &prefix);
		void	mode_o(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &targetNick, const std::string &prefix);
		void	mode_l(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &param, const std::string &prefix);
		void	mode_user(int clientFd, const std::string &target, const std::string &rest);
		bool	handleFileTransferEvent(size_t &i);
		bool	handleFileTransferDataEvent(size_t &i, int curFd, unsigned long tid);
		bool	sendBuffer(std::vector<struct pollfd> &fds, FileTransfer &ft, std::string &buffer, int dst, bool countBytes);
		void	setPollEvents(std::vector<struct pollfd> &fds, int fd, short events);
		void	removePollFd(std::vector<struct pollfd> &fds, int fd);
		void	cleanupClientTransfers(int badfd);

	public:
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);
		~Server();

		bool	InitSocketAndListen(long port, const std::string &password);
		int		setNonBlocking(int fd);
		bool	handleClientEvent(size_t i);
		int		runServerLoop();

};

std::string	convertIntToString(int n);
long		ft_strtol(const char *str, char **endptr);
bool		parseDccIp(const std::string &tok, struct in_addr &out);


#endif