/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileTransfer.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/12 11:52:15 by victor            #+#    #+#             */
/*   Updated: 2026/03/19 16:46:42 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FileTransfer.hpp"

FileTransfer::FileTransfer()
{
	id = 0;
	senderFd = -1;
	receiverFd = -1;
	socketFileTransfer = -1;
	senderFdRedDDC = -1;
	receiverFdRedDDC = -1;
	filename = "";
	filesize = 0;
	bytesTransferred = 0;
	buf_peer_to_remote = "";
	buf_remote_to_peer = "";
	listenerCreated = false;
	bothConnected = false;
	senderClosed = false;
	receiverClosed = false;
	finished = false;
	startedAt = 0;
	lastActivity = 0;
}

FileTransfer::FileTransfer(const FileTransfer &other)
{
	id = other.id;
	senderFd = other.senderFd;
	receiverFd = other.receiverFd;
	socketFileTransfer = -1;
	senderFdRedDDC = -1;
	receiverFdRedDDC = -1;
	filename = other.filename;
	filesize = other.filesize;
	bytesTransferred = other.bytesTransferred;
	buf_peer_to_remote = other.buf_peer_to_remote;
	buf_remote_to_peer = other.buf_remote_to_peer;
	listenerCreated = other.listenerCreated;
	bothConnected = other.bothConnected;
	senderClosed = other.senderClosed;
	receiverClosed = other.receiverClosed;
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
		socketFileTransfer = -1;
		senderFdRedDDC = -1;
		receiverFdRedDDC = -1;
		filename = other.filename;
		filesize = other.filesize;
		bytesTransferred = other.bytesTransferred;
		buf_peer_to_remote = other.buf_peer_to_remote;
		buf_remote_to_peer = other.buf_remote_to_peer;
		listenerCreated = other.listenerCreated;
		bothConnected = other.bothConnected;
		senderClosed = other.senderClosed;
		receiverClosed = other.receiverClosed;
		finished = other.finished;
		startedAt = other.startedAt;
		lastActivity = other.lastActivity;
	}
	return (*this);
}

FileTransfer::~FileTransfer()
{
	closeAll();
}

int FileTransfer::setNonBlocking(int fd)
{
	// int flags = fcntl(fd, F_GETFL, 0);
	// if (flags == -1)
	// 	return (-1);
	// if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	// 	return (-1);
	// return (0);

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		return (-1);
	return (0);
}

// Crear listener para el proxy DCC
int FileTransfer::createListener()
{
	if (socketFileTransfer != -1)
	{
		close(socketFileTransfer);
		socketFileTransfer = -1;
	}

	socketFileTransfer = socket(AF_INET, SOCK_STREAM, 0);
	if (socketFileTransfer < 0)
		return (-1);

	int opt = 1;
	setsockopt(socketFileTransfer, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
	// socketFileTransfer=socket, SOL_SOCKET=aplica a todo el socket(no solo al puerto o ip o protocolo),
	// SO_REUSEADDR=permite reusar ip y puerto, &opt=activarlo, sizeof(opt)=tamaño valor

	if (setNonBlocking(socketFileTransfer) == -1)
	{
		close(socketFileTransfer);
		socketFileTransfer = -1;
		return (-1);
	}

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0); // puerto automático
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(socketFileTransfer, (sockaddr *)&addr, sizeof(addr)) < 0)
	{
		close(socketFileTransfer);
		socketFileTransfer = -1;
		return (-1);
	}

	if (listen(socketFileTransfer, 1) < 0)
	{
		close(socketFileTransfer);
		socketFileTransfer = -1;
		return (-1);
	}

	listenerCreated = true;
	startedAt = time(NULL);
	lastActivity = startedAt;

	return 0;
}

// Obtener puerto asignado
unsigned short FileTransfer::getListenerPort() const
{
	if (socketFileTransfer < 0)
		return (0);

	sockaddr_in addr;
	socklen_t len = sizeof(addr);

	if (getsockname(socketFileTransfer, (sockaddr *)&addr, &len) == -1)
		return (0);

	return (ntohs(addr.sin_port)); //ntohs devuelve el puerto que usa el cliente cuando se conecta al servidor
}

// Cerrar todos los sockets propietarios de la transferencia
void FileTransfer::closeAll()
{
	if (socketFileTransfer != -1)
	{
		close(socketFileTransfer);
		socketFileTransfer = -1;
	}

	if (senderFdRedDDC != -1)
	{
		close(senderFdRedDDC);
		senderFdRedDDC = -1;
	}

	if (receiverFdRedDDC != -1)
	{
		close(receiverFdRedDDC);
		receiverFdRedDDC = -1;
	}

	buf_peer_to_remote.clear();
	buf_remote_to_peer.clear();
	listenerCreated = false;
	bothConnected = false;
	senderClosed = true;
	receiverClosed = true;
	finished = true;
}

// Saber si sigue activa
bool FileTransfer::isActive() const
{
	if (finished)
		return (false);

	if (socketFileTransfer != -1 || senderFdRedDDC != -1 || receiverFdRedDDC != -1)
		return (true);

	return (false);
}
