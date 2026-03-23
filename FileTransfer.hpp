/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileTransfer.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/12 11:52:24 by victor            #+#    #+#             */
/*   Updated: 2026/03/23 14:45:31 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// FileTransfer.hpp
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

/*al enviar un archivo creamos una nueva red que se llama DDC, entonces en ese mommento tenemos dos redes, una la IRC y otra que es DDC*/

class FileTransfer
{
	public:
		unsigned long id;		// id único (Server asigna)
		int senderFd;		// fd del emisor (cliente que solicitó enviar)
		int receiverFd;		// fd del receptor (cliente objetivo)
		int socketFileTransfer;	// socket listening creado por el server (-1 si no)
		int senderFdRedDDC;	// fd del cliente que envia cuando se conecta a la red DDC para la transferencia de archivos
		int receiverFdRedDDC;		// fd del cliente que recibe cuando se conecta a la red DDC para la transferencia de archivos
		std::string filename;
		unsigned long filesize;
		unsigned long bytesTransferred;
		std::string buf_peer_to_remote; /* buffers para reenvío */
		std::string buf_remote_to_peer;  /* buffers para reenvío */
		bool listenerCreated;
		bool bothConnected;
		bool finished;
		bool senderClosed;		// sender DDC cerró su conexión
		bool receiverClosed;		// receiver DDC cerró su conexión
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
		int setNonBlockinging(int fd);
};

#endif