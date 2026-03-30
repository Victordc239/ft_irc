/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileTransfer.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/12 11:52:24 by victor            #+#    #+#             */
/*   Updated: 2026/03/30 11:18:52 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FILETRANSFER_HPP
#define FILETRANSFER_HPP

#include <string>
#include <iostream>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>

class FileTransfer
{
	public:
		unsigned long id;
		int senderFd;
		int receiverFd;
		int socketFileTransfer;
		int senderFdRedDDC;
		int receiverFdRedDDC;
		std::string filename;
		unsigned long filesize;
		unsigned long bytesTransferred;
		std::string buf_peer_to_remote;
		std::string buf_remote_to_peer;
		bool listenerCreated;
		bool bothConnected;
		bool finished;
		bool senderClosed;
		bool receiverClosed;
		time_t startedAt;
		time_t lastActivity;

		FileTransfer();
		FileTransfer(const FileTransfer &other);
		FileTransfer &operator=(const FileTransfer &other);
		~FileTransfer();

		int createListener();
		void closeTransferSockets();
		unsigned short getListenerPort() const;
		bool isTransferActive() const;

	private:
		int setNonBlocking(int fd);
};

#endif