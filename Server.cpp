/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:20 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/30 10:18:01 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server()
{
	_server_fd = -1;
	_serverPassword = "";
	std::memset(_buf, 0, sizeof(_buf));
	_nextTransferId = 1;
}

Server::Server(const Server &other)
{
	_server_fd = -1;
	_serverPassword = other._serverPassword;
	std::memcpy(_buf, other._buf, sizeof(_buf));
	_nextTransferId = 1;
	_fds.clear();
	_clients.clear();
}

Server &Server::operator=(const Server &other)
{
	if (this == &other)
		return (*this);
	if (_server_fd != -1)
	{
		close(_server_fd);
		_server_fd = -1;
	}
	size_t j = 0;
	while (j < _fds.size())
	{
		close(_fds[j].fd);
		++j;
	}
	_fds.clear();
	_clients.clear();
	_serverPassword = other._serverPassword;
	std::memcpy(_buf, other._buf, sizeof(_buf));
	return (*this);
}

Server::~Server()
{
	if (_server_fd != -1)
		close(_server_fd);

	size_t j = 0;
	while (j < _fds.size())
	{
		close(_fds[j].fd);
		++j;
	}
}

bool Server::isNickInUse(const std::string &nick) const
{
	std::map<int, Client>::const_iterator iterator = _clients.begin();

	while (iterator != _clients.end())
	{
		if (iterator->second.nickname == nick)
			return (true);
		++iterator;
	}
	return (false);
}

int Server::findFdByNick(const std::string &nick) const
{
	std::map<int, Client>::const_iterator iterator = _clients.begin();

	while (iterator != _clients.end())
	{
		if (iterator->second.nickname == nick)
			return (iterator->first);
		++iterator;
	}
	return (-1);
}

// Stores and prepares formatted numeric messages for sending from the server to the client
void Server::sendNumericMessage(int fd, const std::string &msg)
{
	if (_clients.find(fd) == _clients.end())
		return;

	std::string full = msg + "\r\n";
	std::string &out = _clients[fd].outBuffer;

	if (out.size() + full.size() > 1048576)
	{
		std::cerr << "WARNING: client fd " << fd << " exceeded 1MB (" << (out.size() + full.size()) << " bytes). Disconnecting.\n";

		cleanupClientTransfers(fd);

		close(fd);
		_clients.erase(fd);
		_fdToTransferId.erase(fd);

		removePollFd(_fds, fd);

		return;
	}
	
	out = out + full;

	if (!out.empty())
	{
		bool found = false;
		size_t j = 0;
		while (j < _fds.size())
		{
			if (_fds[j].fd == fd)
			{
				_fds[j].events |= POLLOUT;
				found = true;
				break;
			}
			++j;
		}

		if (!found)
		{
			pollfd p;
			p.fd = fd;
			p.events = POLLIN | POLLOUT;
			p.revents = 0;
			_fds.push_back(p);
		}
	}
	else
	{
		size_t j = 0;
		while (j < _fds.size())
		{
			if (_fds[j].fd == fd)
			{
				_fds[j].events &= ~POLLOUT;
				break;
			}
			++j;
		}
	}
}

// By default, if a client does not send data, it could block other clients from trying to connect or send messages
int Server::setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		return (-1);
	return (0);
}

bool Server::InitSocketAndListen(long port, const std::string &password)
{
	_serverPassword = password;

	_server_fd = socket(AF_INET, SOCK_STREAM, 0); //AF_INET = IPV4, SOCK_STREAM = TCP, 0 = default protocol
	if (_server_fd == -1)
	{
		std::cerr << "ERROR: socket failure\n";
		return (false);
	}

	std::cout << "Socket created successfully\n";
	// Setsockopt allows the server to reuse the same port immediately after closing, 
	// avoiding errors from the OS keeping the port in TIME_WAIT
	int opt = 1;
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		// server_fd = socket fd, SOL_SOCKET = the entire socket, SO_REUSEADDR = It allows you to reuse IP address and port, &opt = activate it
		std::cerr << "ERROR: setsockopt failed\n";

		if (_server_fd != -1)
		{
			close(_server_fd);
			_server_fd = -1;
		}
		size_t jj = 0;
		while (jj < _fds.size())
		{
			close(_fds[jj].fd);
			++jj;
		}
		_fds.clear();
		_clients.clear();

		return (false);
	}
	
	if (Server::setNonBlocking(_server_fd) == -1)
	{
		std::cerr << "ERROR: The server could not be set to non-blocking\n";

		if (_server_fd != -1)
		{
			close(_server_fd);
			_server_fd = -1;
		}
		size_t jj = 0;
		while (jj < _fds.size())
		{
			close(_fds[jj].fd);
			++jj;
		}
		_fds.clear();
		_clients.clear();

		return (false);
	}
	
	sockaddr_in server_addr; // Save ip and port
	std::memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; //IPv4
	server_addr.sin_port = htons((uint16_t)port); //Converting the port number into bits so that all computers interpret it the same way
	server_addr.sin_addr.s_addr = INADDR_ANY; // Ethernet, wifi, localhost,...

	if (bind(_server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) //Associate the port and IP address to the socket
	{
		std::cerr << "ERROR: bind failure\n";
		close(_server_fd);
		_server_fd = -1;
		return (false);
	}

	std::cout << "Bind done correctly\n";

	if (listen(_server_fd, SOMAXCONN) == -1) //SOMAXCONN = max number of clients the server can listen to
	{
		std::cerr << "ERROR: listen failure\n";
		close(_server_fd);
		_server_fd = -1;
		return (false);
	}

	std::cout << "Server listening...\n";

	pollfd pfd;
	pfd.fd = _server_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_fds.push_back(pfd);

	return (true);
}

// Executes commands, registers users, and manages disconnections
bool Server::handleClientEvent(size_t i)
{
	if (!(_fds[i].revents & POLLIN))
		return (false);

	int clientFd = _fds[i].fd;
	ssize_t n = recv(clientFd, _buf, BUF_SIZE, 0); //number of bytes that the client has sent to the server

	if (n > 0)
	{
		_clients[clientFd].accumulator.append(_buf, _buf + n);
		size_t pos;
		while ((pos = _clients[clientFd].accumulator.find('\n')) != std::string::npos)
		{
			std::string line = _clients[clientFd].accumulator.substr(0, pos);
			if (!line.empty() && line[line.size() - 1] == '\r')
				line.erase(line.size() - 1);

			std::cout << "fd " << clientFd << " -> Line received: [" << line << "]\n";
			if (!_clients[clientFd].registered)
			{
				// Unregistered USER
				handleAuthenticationCmds(i, line);
				if (line.compare(0, 5, "NICK ") == 0)
					handleNickCommand(clientFd, line);
				else if (line.compare(0, 5, "USER ") == 0)
					handleUserCommand(clientFd, line);
				else if (line.compare(0, 4, "JOIN") == 0 || line.compare(0, 7, "PRIVMSG") == 0 || line.compare(0, 4, "KICK") == 0 || line.compare(0, 6, "INVITE") == 0
						|| line.compare(0, 5, "TOPIC") == 0 || line.compare(0, 4, "MODE") == 0 || line.compare(0, 4, "PART") == 0)
				{
					Client &client = _clients[clientFd];
					std::cout << "fd " << clientFd << " tried to use [" << line << "] without registering. Falta: ";
					if (!client.correctPass)
						std::cout << "PASS ";
					if (client.nickname.empty())
						std::cout << "NICK ";
					if (client.username.empty())
						std::cout << "USER ";
					std::cout << std::endl;
					sendNumericMessage(clientFd, "451 :You have not registered"); // ERR_NOTREGISTERED 451
				}

				// Registered USER = PASS check+ USER check + NICK check
				if (_clients[clientFd].correctPass && !_clients[clientFd].nickname.empty() && !_clients[clientFd].username.empty())
				{
					_clients[clientFd].registered = true;
					std::string welcome = "001 " + _clients[clientFd].nickname + " :Welcome to the simple IRCd";
					sendNumericMessage(clientFd, welcome);
					std::cout << "fd " << clientFd << " registered (PASS+NICK+USER). Sent 001.\n"; // RPL_WELCOME (001)
				}
			}
			else
			{
				if (line.compare(0, 4, "CAP ") == 0)
					handleAuthenticationCmds(i, line);
				else if (line.compare(0, 5, "JOIN ") == 0)
					handleJoinCommand(clientFd, line);
				else if (line.compare(0, 5, "PART ") == 0)
					handlePartCommand(clientFd, line);
				else if (line.compare(0, 8, "PRIVMSG ") == 0)
					handlePrivmsgCommand(clientFd, line);
				else if (line.compare(0, 5, "KICK ") == 0)
					handleKickCommand(clientFd, line);
				else if (line.compare(0, 7, "INVITE ") == 0)
					handleInviteCommand(clientFd, line);
				else if (line.compare(0, 6, "TOPIC ") == 0)
					handleTopicCommand(clientFd, line);
				else if (line.compare(0, 5, "MODE ") == 0)
					handleChannelModes(clientFd, line);
				else if (line.compare(0, 5, "PING ") == 0)
				{
					std::string ping_target = line.substr(5);
					sendNumericMessage(clientFd, "PONG " + ping_target);
					std::cout << "fd " << clientFd << " -> Responds PONG to [" << ping_target << "]\n";
				}
				else if (line.compare(0, 5, "PASS ") == 0 || line.compare(0, 5, "USER ") == 0)
				{
					std::string cur = _clients[clientFd].nickname;
					if (cur.empty())
						cur = "*";
					sendNumericMessage(clientFd, ":ircserv 462 " + cur + " :You may not reregister"); // ERR_ALREADYREGISTERED 462
				}
				else if (line.compare(0, 5, "NICK ") == 0)
				{
					std::string originalNick = _clients[clientFd].nickname;
					std::string displayOldNick;
					if (originalNick.empty())
						displayOldNick = convertIntToString(clientFd);
					else
						displayOldNick = originalNick;

					std::string user;
					if (_clients[clientFd].username.empty())
						user = "user";
					else
						user = _clients[clientFd].username;

					std::string oldPrefix = displayOldNick + "!" + user + "@localhost";
					handleNickCommand(clientFd, line);
					std::string newNick = _clients[clientFd].nickname;
					if (newNick != originalNick && !newNick.empty())
					{
						std::string out = ":" + oldPrefix + " NICK " + newNick;
						std::map<int, Client>::iterator it = _clients.begin();
						while (it != _clients.end())
						{
							sendNumericMessage(it->first, out);
							++it;
						}
					}
				}
				else
					sendNumericMessage(clientFd, "421 :Unknown command");
			}
			_clients[clientFd].accumulator.erase(0, pos + 1);
		}
	}
	else if (n == 0) // The client closed the connection.
	{
		int closedFd = clientFd;
		std::cout << "Client (fd " << closedFd << ") closed the connection\n";
		cleanupClientTransfers(closedFd);
		close(closedFd);
		_clients.erase(closedFd);
		_fdToTransferId.erase(closedFd);
		_fds.erase(_fds.begin() + i);
		return (true);
	}
	else // There's an error, we're closing everything.
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		{
			std::cerr << "ERROR: recv failed\n";
			int bad = clientFd;
			cleanupClientTransfers(bad);
			close(clientFd);
			_clients.erase(clientFd);
			_fdToTransferId.erase(clientFd);
			_fds.erase(_fds.begin() + i);
			return (true);
		}
	}
	return (false);
}

// Principal loop
int Server::runServerLoop()
{
	while (g_running)
	{
		int ret = poll(&_fds[0], _fds.size(), -1); // Wait for some event to occur on a client or server
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "ERROR: poll failed\n";
			break;
		}
		size_t i = 0;
		while (i < _fds.size())
		{
			if (_fds[i].revents == 0)
			{
				++i;
				continue;
			}
			if (handleFileTransferEvent(i))
				continue;
			if (_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) // ERROR or disconnection. POLLERR=broken connection, POLLHUP=close the terminal, POLLNVAL=corrupted fd
			{
				int badfd = _fds[i].fd;
				if (badfd == _server_fd)
				{
					std::cerr << "ERROR in poll socket\n";
					g_running = 0;
					break;
				}
				cleanupClientTransfers(badfd); 
				std::cout << "Client (fd " << badfd << ") disconnected/err\n";
				close(badfd);
				_clients.erase(badfd);
				_fdToTransferId.erase(badfd);
				_fds.erase(_fds.begin() + i);
				continue;
			}
			if (_fds[i].fd == _server_fd && (_fds[i].revents & POLLIN)) // NEW conexion
			{
				while (true)
				{
					sockaddr_in client_addr;
					socklen_t client_len = sizeof(client_addr);
					int client_fd = accept(_server_fd, (sockaddr*)&client_addr, &client_len);
					if (client_fd == -1)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;
						std::cerr << "ERROR in accept new client\n";
						break;
					}
					if (Server::setNonBlocking(client_fd) == -1)
					{
						std::cerr << "WARNING: The client_fd could not be set to non-blocking\n";
						close(client_fd);
						break;
					}
					pollfd cp;
					cp.fd = client_fd;
					cp.events = POLLIN;
					cp.revents = 0;
					_fds.push_back(cp);
					_clients[client_fd] = Client(client_fd);

					char client_ip[INET_ADDRSTRLEN];
					if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL)
						std::cout << "Client connected (fd " << client_fd << ") from [unknown IP address]:" << ntohs(client_addr.sin_port) << "!\n"; 
					else
						std::cout << "Client connected from " << client_ip << ":" << ntohs(client_addr.sin_port) << " (fd " << client_fd << ")\n";
				}
				++i;
				continue;
			}
			if (handleClientEvent(i)) // Normal reading at client (POLLIN)
				continue;
			if (_fds[i].revents & POLLOUT) // Send buffer from server->client (POLLOUT)
			{
				int fd = _fds[i].fd;
				if (_clients.find(fd) == _clients.end())
				{
					close(fd);
					_fds.erase(_fds.begin() + i);
					continue;
				}

				std::string &data = _clients[fd].outBuffer;
				while (!data.empty())
				{
					ssize_t sent = send(fd, data.c_str(), data.size(), 0);

					if (sent > 0)
						data.erase(0, sent);
					else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
						break;
					else
					{
						std::cerr << "ERROR: send failed\n";
						int badfd = fd;
						close(badfd);
						_clients.erase(badfd);
						_fdToTransferId.erase(badfd);
						cleanupClientTransfers(badfd);
						_fds.erase(_fds.begin() + i);
						continue;
					}
				}
				//If I've finished sending everything to the client, put POLLOUT
				if (_clients.find(fd) != _clients.end() && _clients[fd].outBuffer.empty())
					_fds[i].events &= ~POLLOUT;
			}
			++i;
		}
	}
	std::string shutdown_msg = "ERROR :Server is shutting down\r\n";
	std::map<int, Client>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		int fd = it->first;
		ssize_t s = send(fd, shutdown_msg.c_str(), shutdown_msg.size(), 0);
		(void)s;
		++it;
	}
	size_t j = 0;
	while (j < _fds.size())
	{
		close(_fds[j].fd);
		++j;
	}
	return (0);
}
