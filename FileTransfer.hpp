/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileTransfer.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/12 11:52:24 by victor            #+#    #+#             */
/*   Updated: 2026/03/16 12:03:13 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// FileTransfer.hpp
#ifndef FILETRANSFER_HPP
#define FILETRANSFER_HPP

#include <string>
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
		unsigned long id;       // id único (Server asigna)
		int senderFd;           // fd del emisor (cliente que solicitó enviar)
		int receiverFd;         // fd del receptor (cliente objetivo)
		int listenerFd;         // socket listening creado por el server (-1 si no)
		int peerFd;             // primer accept() (-1 si no)
		int remoteFd;           // segundo socket de la transferencia (-1 si no)
		std::string filename;
		unsigned long filesize;
		unsigned long bytesTransferred;
		std::string buf_peer_to_remote; /* buffers para reenvío */
		std::string buf_remote_to_peer;  /* buffers para reenvío */
		bool listenerCreated;
		bool bothConnected;
		bool finished;
		time_t startedAt;	// timestamp de inicio
		time_t lastActivity;	// ultimo timestamp

		FileTransfer();
		FileTransfer(const FileTransfer &other);
		FileTransfer &operator=(const FileTransfer &other);
		~FileTransfer();

		// Crear listener en INADDR_ANY puerto 0 (ephemeral), devuelve 0 ok, -1 error
		int createListener();

		// Cierra los sockets propios (listener, peer, remote) y marca finished=true
		void closeAll();

		// Obtener puerto asignado al listener (0 si no hay listener)
		unsigned short getListenerPort() const;

		// True si aún está activo (no finished y al menos listener o sockets vivos)
		bool isActive() const;

	private:
		int setNonBlocking(int fd);
};

#endif