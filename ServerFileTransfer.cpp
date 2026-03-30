/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerFileTransfer.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 14:14:47 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/30 15:03:27 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

void	Server::removePollFd(std::vector<struct pollfd> &fds, int fd)
{
	size_t i = 0;
	while (i < fds.size())
	{
		if (fds[i].fd == fd)
		{
			fds.erase(fds.begin() + i);
			return;
		}
		++i;
	}
}

void	Server::setPollEvents(std::vector<struct pollfd> &fds, int fd, short events)
{
	size_t i = 0;
	while (i < fds.size())
	{
		if (fds[i].fd == fd)
		{
			fds[i].events = events;
			return;
		}
		++i;
	}
}

bool	Server::sendBuffer(std::vector<struct pollfd> &fds, FileTransfer &ft, std::string &buffer, int dst, bool countBytes)
{
	if (dst == -1)
		return (true);

	while (!buffer.empty())
	{
		ssize_t sent = send(dst, buffer.c_str(), buffer.size(), 0);
		if (sent > 0)
		{
			if (countBytes)
				ft.bytesTransferred = ft.bytesTransferred + (unsigned long)sent;
			buffer.erase(0, sent);
		}
		else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			setPollEvents(fds, dst, POLLIN | POLLOUT);
			return (true);
		}
		else
		{
			std::cerr << "Error\n";
			return (false);
		}
	}
	setPollEvents(fds, dst, POLLIN);
	return (true);
}

bool Server::handleFileTransferEvent(size_t &i)
{
	int curFd = _fds[i].fd;
	std::map<int, unsigned long>::iterator itmap = _fdToTransferId.find(curFd);
	if (itmap != _fdToTransferId.end())
	{
		unsigned long tid = itmap->second;
		std::map<unsigned long, FileTransfer>::iterator itft = _transfers.find(tid);
		if (itft == _transfers.end())
		{
			_fdToTransferId.erase(itmap);
			++i;
			return (true);
		}
		FileTransfer &ft = itft->second;

		// If the fd is the listener and there is POLLIN => accept()
		if (curFd == ft.socketFileTransfer && (_fds[i].revents & POLLIN))
		{
			struct sockaddr_in peerAddr;
			socklen_t alen = sizeof(peerAddr);
			int newfd = accept(ft.socketFileTransfer, (struct sockaddr*)&peerAddr, &alen);
			if (newfd != -1)
			{
				if (Server::setNonBlocking(newfd) == -1)
				{
					close(newfd);
					newfd = -1;
				}
				else
				{
					//  This listener is for the receiver, not for the sender
					if (ft.receiverFdRedDDC == -1)
					{
						ft.receiverFdRedDDC = newfd;
						ft.receiverClosed = false;
					}
					else
					{
						close(newfd);
						newfd = -1;
					}

					if (newfd != -1)
					{
						pollfd p;
						p.fd = newfd;
						p.events = POLLIN;
						p.revents = 0;
						_fds.push_back(p);
						_fdToTransferId[newfd] = tid;
					}

					if (ft.senderFdRedDDC != -1 && ft.receiverFdRedDDC != -1)
					{
						ft.bothConnected = true;
						ft.lastActivity = std::time(NULL);

						if (!ft.buf_peer_to_remote.empty())
						{
							if (!sendBuffer(_fds, ft, ft.buf_peer_to_remote, ft.receiverFdRedDDC, true))
								ft.closeTransferSockets();
						}

						if (_clients.find(ft.senderFd) != _clients.end())
						{
							std::string sNick = _clients[ft.senderFd].nickname;
							if (sNick.empty())
								sNick = convertIntToString(ft.senderFd);
							sendNumericMessage(ft.senderFd, ":ircserv NOTICE " + sNick + " :DCC proxy connection established for id=" + convertIntToString((int)ft.id));
						}
					}
				}
			}
			++i;
			return (true);
		}

		if (curFd == ft.senderFdRedDDC && (_fds[i].revents & POLLOUT))
		{
			int detectErr = 0;
			char peekbuf;
			ssize_t pr = recv(curFd, &peekbuf, 1, MSG_PEEK | MSG_DONTWAIT);

			if (pr == -1)
			{
				if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
					detectErr = errno;
			}
			else if (pr == 0)
				detectErr = ECONNRESET;

			if (detectErr != 0)
			{
				std::cerr << "ERROR: ft id=" << ft.id << " sender connect failed\n";
				ft.closeTransferSockets();
				++i;
				return (true);
			}

			setPollEvents(_fds, curFd, POLLIN);
			ft.bothConnected = (ft.receiverFdRedDDC != -1);

			if (ft.receiverFdRedDDC != -1 && !ft.buf_peer_to_remote.empty())
			{
				if (!sendBuffer(_fds, ft, ft.buf_peer_to_remote, ft.receiverFdRedDDC, true))
					ft.closeTransferSockets();
			}

			++i;
			return (true);
		}

		if (handleFileTransferDataEvent(i, curFd, tid))
			return (true);
	}
	return (false);
}

// Processes the reading, writing and cleaning of a file transfer 
bool Server::handleFileTransferDataEvent(size_t &i, int curFd, unsigned long tid)
{
	std::map<unsigned long, FileTransfer>::iterator itft = _transfers.find(tid);
	if (itft == _transfers.end())
	{
		++i;
		return (true);
	}
	FileTransfer &ft = itft->second;

	bool isPeer = (curFd == ft.senderFdRedDDC);
	bool isRemote = (curFd == ft.receiverFdRedDDC);

	if ((isPeer || isRemote) && (_fds[i].revents & POLLIN))
	{
		char tmpbuf[4096];
		ssize_t rn = recv(curFd, tmpbuf, sizeof(tmpbuf), 0);
		if (rn > 0)
		{
			ft.lastActivity = std::time(NULL);

			if (isPeer)
				ft.buf_peer_to_remote.append(tmpbuf, tmpbuf + rn);
			else
				ft.buf_remote_to_peer.append(tmpbuf, tmpbuf + rn);

			int dst;
			if (isPeer)
				dst = ft.receiverFdRedDDC;
			else
				dst = ft.senderFdRedDDC;

			std::string *outBuffer;
			if (isPeer)
				outBuffer = &ft.buf_peer_to_remote;
			else
				outBuffer = &ft.buf_remote_to_peer;

			if (dst != -1)
			{
				bool countBytes;
				if (isPeer)
					countBytes = true;
				else
					countBytes = false;
				if (!sendBuffer(_fds, ft, *outBuffer, dst, countBytes))
					ft.closeTransferSockets();
			}
		}
		else if (rn == 0)
		{
			if (isPeer)
			{
				removePollFd(_fds, ft.senderFdRedDDC);
				close(ft.senderFdRedDDC);
				ft.senderFdRedDDC = -1;
				ft.senderClosed = true;
			}
			else
			{
				ft.receiverClosed = true;
				ft.closeTransferSockets();
			}
		}
		else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			ft.closeTransferSockets();
		++i;
		return (true);
	}

	// POLLOUT
	if ((isPeer || isRemote) && (_fds[i].revents & POLLOUT))
	{
		std::string *outBuffer;
		if (isPeer)
			outBuffer = &ft.buf_remote_to_peer;
		else
			outBuffer = &ft.buf_peer_to_remote;

		bool countBytes;
		if (isPeer)
			countBytes = false;
		else
			countBytes = true;

		if (!sendBuffer(_fds, ft, *outBuffer, curFd, countBytes))
			ft.closeTransferSockets();

		if (outBuffer->empty())
			setPollEvents(_fds, curFd, POLLIN);
		++i;
		return (true);
	}

	// Cleanup
	if (!ft.isTransferActive())
	{
		size_t k = 0;
		while (k < _fds.size())
		{
			int fdk = _fds[k].fd;
			if (fdk == ft.socketFileTransfer || fdk == ft.senderFdRedDDC || fdk == ft.receiverFdRedDDC)
			{
				_fdToTransferId.erase(fdk);
				close(fdk);
				_fds.erase(_fds.begin() + k);
				continue;
			}
			++k;
		}
		_transfers.erase(tid);
		++i;
		return (true);
	}
	return (false);
}

// Interpret an IP address sent to a DCC and transform it into a format usable by the socket
bool	parseDccIp(const std::string &tok, struct in_addr &out)
{
	// DCC style decimal IP address (ex: 2130706433)
	char *endptr = NULL;
	unsigned long ip_dec = strtoul(tok.c_str(), &endptr, 10);
	if (!tok.empty() && *endptr == '\0')
	{
		out.s_addr = htonl((uint32_t)ip_dec);
		return (true);
	}

	// Normal IP address with dots (ex: 127.0.0.1)
	out.s_addr = inet_addr(tok.c_str());
	if (out.s_addr == INADDR_NONE && tok != "255.255.255.255")
		return (false);

	return (true);
}

// Prepare the DCC transfer so that the receiver connects to the proxy instead of directly to the sender
bool Server::handleDccSendProxy(int clientFd, int dst_fd, const std::vector<std::string> &toks, const std::string &prefix, const std::string &target, const std::string &filename, unsigned long fsize, unsigned long transferId)
{
	// Try to recover original sender IP address and PORT if they were in tokens
	if (toks.size() >= 3)
	{
		std::string orig_ip_tok = toks[1];
		std::string orig_port_tok = toks[2];
		struct sockaddr_in sender_addr;
		std::memset(&sender_addr, 0, sizeof(sender_addr));
		sender_addr.sin_family = AF_INET;
		bool haveSenderAddr = false;

		struct in_addr ina;
		if (parseDccIp(orig_ip_tok, ina))
		{
			sender_addr.sin_addr = ina;
			haveSenderAddr = true;
		}

		int sender_port = 0;
		if (haveSenderAddr)
		{
			char *endptr_port = NULL;
			sender_port = (int)strtol(orig_port_tok.c_str(), &endptr_port, 10);
			if (*endptr_port != '\0' || sender_port <= 0 || sender_port > 65535)
				haveSenderAddr = false;
			else
				sender_addr.sin_port = htons((uint16_t)sender_port);
		}

		if (haveSenderAddr)
		{
			// Create socket and connect to the sender (non-blocking)
			int s = socket(AF_INET, SOCK_STREAM, 0);
			if (s != -1)
			{
				fcntl(s, F_SETFL, O_NONBLOCK);

				int cres = connect(s, (struct sockaddr*)&sender_addr, sizeof(sender_addr));
				if (cres == 0)
				{
					_transfers[transferId].senderFdRedDDC = s;
					_transfers[transferId].senderClosed = false;

					pollfd sp;
					sp.fd = s;
					sp.events = POLLIN;
					sp.revents = 0;
					_fds.push_back(sp);

					_fdToTransferId[s] = transferId;
				}
				else if (errno == EINPROGRESS || errno == EINTR)
				{
					_transfers[transferId].senderFdRedDDC = s;
					_transfers[transferId].senderClosed = false;

					pollfd sp;
					sp.fd = s;
					sp.events = POLLIN | POLLOUT;
					sp.revents = 0;
					_fds.push_back(sp);

					_fdToTransferId[s] = transferId;
				}
				else
					close(s);
			}
			else
				sendNumericMessage(clientFd, ":ircserv NOTICE :DCC proxy could not create outbound socket to sender; proxy will wait for connections");
		}
		else
			sendNumericMessage(clientFd, ":ircserv NOTICE :DCC proxy couldn't parse sender address from CTCP; proxy will wait for incoming connections");
	}
	// Get IP address from server to send to receiver 
	std::string serverIp = "127.0.0.1";
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock != -1)
	{
		struct sockaddr_in remote;
		std::memset(&remote, 0, sizeof(remote));
		remote.sin_family = AF_INET;
		// Connect to a public IP address only to know the outgoing interface
		remote.sin_addr.s_addr = inet_addr("8.8.8.8");
		remote.sin_port = htons(53);
		// Connect does not send packets in UDP, just let choose interface
		if (connect(sock, (struct sockaddr*)&remote, sizeof(remote)) != -1)
		{
			struct sockaddr_in name;
			socklen_t namelen = sizeof(name);
			if (getsockname(sock, (struct sockaddr*)&name, &namelen) != -1)
			{
				char ipbuf[INET_ADDRSTRLEN];
				if (inet_ntop(AF_INET, &name.sin_addr, ipbuf, sizeof(ipbuf)) != NULL)
					serverIp = ipbuf;
			}
		}
		close(sock);
	}

	uint32_t ip_net = inet_addr(serverIp.c_str()); // network order
	uint32_t ip_decimal = ntohl(ip_net);          // host order decimal

	unsigned short port = _transfers[transferId].getListenerPort();
	if (port == 0)
	{
		// fallback: invalid port
		sendNumericMessage(clientFd, ":ircserv NOTICE :DCC proxy internal error (listener port=0)");
		return (true);
	}
	// DCC format: IP in decimal (host order) and port in decimal
	std::string dccmsg = "\001DCC SEND " + filename + " " + convertIntToString((int)ip_decimal) + " " + convertIntToString((int)port) + " " + convertIntToString((int)fsize) + "\001";
	std::string outmsg = ":" + prefix + " PRIVMSG " + target + " :" + dccmsg;

	sendNumericMessage(dst_fd, outmsg);

	std::string senderNick = _clients[clientFd].nickname;
	if (senderNick.empty())
		senderNick = convertIntToString(clientFd);
	sendNumericMessage(clientFd, ":ircserv NOTICE " + senderNick + " :DCC proxy created id=" + convertIntToString((int)transferId));

	return (true);
}

// Intercept a DCC transmission, create the transfer, and prepare the proxy to handle it.
bool Server::handleDccSend(int clientFd, int dst_fd, const std::string &text, const std::string &prefix, const std::string &target)
{
	const std::string dccPrefix = "DCC SEND ";
	size_t pos = 1;
	size_t start = pos + dccPrefix.size();
	std::string rest = "";
	if (start < text.size())
		rest = text.substr(start);

	if (!rest.empty() && rest[rest.size() - 1] == '\001')
		rest.erase(rest.size() - 1);

	std::vector<std::string> toks;
	std::string s = rest;
	while (!s.empty())
	{
		while (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
			s.erase(0, 1);
		if (s.empty())
			break;
		size_t p = s.find(' ');
		if (p == std::string::npos)
		{
			toks.push_back(s);
			break;
		}
		toks.push_back(s.substr(0, p));
		s.erase(0, p + 1);
	}

	if (!toks.empty())
	{
		std::string filename = toks[0];
		unsigned long fsize = 0;

		if (toks.size() >= 4)
		{
			char *endptr = NULL;
			fsize = (unsigned long)ft_strtol(toks[3].c_str(), &endptr);
			if (*endptr != '\0')
				fsize = 0;
		}
		FileTransfer ft;
		ft.id = _nextTransferId++;
		ft.senderFd = clientFd;
		ft.receiverFd = dst_fd;
		ft.filename = filename;
		ft.filesize = fsize;
		ft.bytesTransferred = 0;
		ft.senderClosed = false;
		ft.receiverClosed = false;
		ft.listenerCreated = false;
		ft.bothConnected = false;

		if (ft.createListener() != 0)
		{
			sendNumericMessage(clientFd, ":ircserv NOTICE :DCC proxy failed to create listener");
			std::string fallback = ":" + prefix + " PRIVMSG " + target + " :" + text;
			sendNumericMessage(dst_fd, fallback);
			return (true);
		}
	
		_transfers[ft.id] = ft;
		_transfers[ft.id].socketFileTransfer = ft.socketFileTransfer;
		ft.socketFileTransfer = -1;

		int lfd = _transfers[ft.id].socketFileTransfer;
		if (lfd == -1)
		{
			sendNumericMessage(clientFd, ":ircserv NOTICE :DCC proxy internal error (no listener fd)");
			return (true);
		}

		// Add the listener to the poll array for runServerLoop() to manage
		pollfd pfd;
		pfd.fd = lfd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_fds.push_back(pfd);

		_fdToTransferId[lfd] = ft.id;

		bool result = handleDccSendProxy(clientFd, dst_fd, toks, prefix, target, filename, fsize, ft.id);
		return (result);
	}

	return (false);
}

// Closes and deletes all file transfers associated with a disconnected or invalid client
void Server::cleanupClientTransfers(int badfd)
{
	std::vector<unsigned long> toEraseTids;
	std::map<unsigned long, FileTransfer>::iterator ittr = _transfers.begin();
	while (ittr != _transfers.end())
	{
		unsigned long candTid = ittr->first;
		FileTransfer &cand = ittr->second;
		if (cand.senderFd == badfd || cand.receiverFd == badfd)
		{
			int lfd = cand.socketFileTransfer;
			int pfd = cand.senderFdRedDDC;
			int rfd = cand.receiverFdRedDDC;

			// Close resources
			cand.closeTransferSockets();

			if (lfd != -1) 
				_fdToTransferId.erase(lfd);
			if (pfd != -1)
				_fdToTransferId.erase(pfd);
			if (rfd != -1)
				_fdToTransferId.erase(rfd);

			size_t kk = 0;
			while (kk < _fds.size())
			{
				int fdk = _fds[kk].fd;
				if (fdk == lfd || fdk == pfd || fdk == rfd)
				{
					if (fdk >= 0)
						close(fdk);
					_fds.erase(_fds.begin() + kk);
					continue;
				}
				++kk;
			}
			toEraseTids.push_back(candTid);
		}
		++ittr;
	}
	size_t x = 0;
	while (x < toEraseTids.size())
	{
		_transfers.erase(toEraseTids[x]);
		++x;
	}
}
