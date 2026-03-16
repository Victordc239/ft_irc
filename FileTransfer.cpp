/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileTransfer.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/12 11:52:15 by victor            #+#    #+#             */
/*   Updated: 2026/03/16 12:03:07 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FileTransfer.hpp"

FileTransfer::FileTransfer()
{
	id = 0;
	senderFd = -1;
	receiverFd = -1;
	listenerFd = -1;
	peerFd = -1;
	remoteFd = -1;
	filename = "";
	filesize = 0;
	bytesTransferred = 0;
	buf_peer_to_remote = "";
	buf_remote_to_peer = "";
	listenerCreated = false;
	bothConnected = false;
	finished = false;
	startedAt = 0;
	lastActivity = 0;
}

FileTransfer::FileTransfer(const FileTransfer &other)
{
	id = other.id;
	senderFd = other.senderFd;
	receiverFd = other.receiverFd;
	listenerFd = -1;
	peerFd = -1;
	remoteFd = -1;
	filename = other.filename;
	filesize = other.filesize;
	bytesTransferred = other.bytesTransferred;
	buf_peer_to_remote = other.buf_peer_to_remote;
	buf_remote_to_peer = other.buf_remote_to_peer;
	listenerCreated = other.listenerCreated;
	bothConnected = false;
	finished = other.finished;
	startedAt = other.startedAt;
	lastActivity = other.lastActivity;
}

FileTransfer &FileTransfer::operator=(const FileTransfer &other)
{
	if (this != &other)
	{
		// cerrar recursos propios (si existían) antes de reasignar metadata
		closeAll();

		id = other.id;
		senderFd = other.senderFd;
		receiverFd = other.receiverFd;
		listenerFd = -1;
		peerFd = -1;
		remoteFd = -1;
		filename = other.filename;
		filesize = other.filesize;
		bytesTransferred = other.bytesTransferred;
		buf_peer_to_remote = other.buf_peer_to_remote;
		buf_remote_to_peer = other.buf_remote_to_peer;
		listenerCreated = other.listenerCreated;
		bothConnected = false;
		finished = other.finished;
		startedAt = other.startedAt;
		lastActivity = other.lastActivity;
	}
	return *this;
}

FileTransfer::~FileTransfer()
{
	closeAll();
}

int FileTransfer::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		return -1;
	return 0;
}

/* Crear listener para el proxy DCC */
int FileTransfer::createListener()
{
	if (listenerFd != -1)
	{
		close(listenerFd);
		listenerFd = -1;
	}

	listenerFd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenerFd < 0)
		return -1;

	int opt = 1;
	setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (setNonBlocking(listenerFd) == -1)
	{
		close(listenerFd);
		listenerFd = -1;
		return -1;
	}

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0); // puerto automático
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(listenerFd, (sockaddr *)&addr, sizeof(addr)) < 0)
	{
		close(listenerFd);
		listenerFd = -1;
		return -1;
	}

	if (listen(listenerFd, 1) < 0)
	{
		close(listenerFd);
		listenerFd = -1;
		return -1;
	}

	listenerCreated = true;
	startedAt = time(NULL);
	lastActivity = startedAt;

	return 0;
}

/* Obtener puerto asignado */
unsigned short FileTransfer::getListenerPort() const
{
	if (listenerFd < 0)
		return 0;

	sockaddr_in addr;
	socklen_t len = sizeof(addr);

	if (getsockname(listenerFd, (sockaddr *)&addr, &len) == -1)
		return 0;

	return ntohs(addr.sin_port);
}

/* Cerrar todos los sockets propietarios de la transferencia */
void FileTransfer::closeAll()
{
	if (listenerFd != -1)
	{
		close(listenerFd);
		listenerFd = -1;
	}

	if (peerFd != -1)
	{
		close(peerFd);
		peerFd = -1;
	}

	if (remoteFd != -1)
	{
		close(remoteFd);
		remoteFd = -1;
	}

	buf_peer_to_remote.clear();
	buf_remote_to_peer.clear();
	listenerCreated = false;
	bothConnected = false;
	finished = true;
}

/* Saber si sigue activa */
bool FileTransfer::isActive() const
{
	if (finished)
		return false;

	if (listenerFd != -1 || peerFd != -1 || remoteFd != -1)
		return true;

	return false;
}
