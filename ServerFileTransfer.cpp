/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerFileTransfer.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 14:14:47 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/16 23:03:46 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

/* Pequeños helpers internos para no repetir código */
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

bool	Server::flushBufferToFd(std::vector<struct pollfd> &fds, FileTransfer &ft, std::string &buffer, int dst, bool countBytes)
{
	if (dst == -1)
		return (true);

	while (!buffer.empty())
	{
		ssize_t sent = send(dst, buffer.c_str(), buffer.size(), 0);
		if (sent > 0)
		{
			if (countBytes)
				ft.bytesTransferred += (unsigned long)sent;
			buffer.erase(0, sent);
		}
		else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			// no podemos escribir ahora, asegurarnos de vigilar POLLOUT del dst
			setPollEvents(fds, dst, POLLIN | POLLOUT);
			return (true);
		}
		else
		{
			std::cerr << "Error\n";
			return (false);
		}
	}

	// Si el buffer ya quedó vacío, quitamos POLLOUT del destino
	setPollEvents(fds, dst, POLLIN);
	return (true);
}

/* Maneja eventos relacionados con FileTransfer para el fd en _fds[i].
   Si el fd actual pertenece a una transferencia, lo procesa (accept, relay, out,
   y limpieza cuando la transferencia termina). Actualiza i según el flujo original
   (se realizan ++i y continue en los puntos correspondientes).
   Devuelve true si se procesó el evento como parte de FileTransfer (y por tanto
   el caller debe 'continue' el bucle), false si el fd no pertenece a ninguna transferencia.
*/
bool Server::handleFileTransferEvent(size_t &i)
{
	// --- Manejo de fds asociados a FileTransfer ---
	// Este bloque prioriza eventos de transferencias de archivos (listeners, peer, remote).
	// Si el fd actual pertenece a una transferencia, lo manejamos aquí y continuamos.
	int curFd = _fds[i].fd;
	std::map<int, unsigned long>::iterator itmap = _fdToTransferId.find(curFd);
	if (itmap != _fdToTransferId.end())
	{
		unsigned long tid = itmap->second;
		std::map<unsigned long, FileTransfer>::iterator itft = _transfers.find(tid);
		if (itft == _transfers.end())
		{
			// mapping huérfano -> limpiar y seguir
			_fdToTransferId.erase(itmap);
			++i;
			return (true);
		}
		FileTransfer &ft = itft->second;

		// 1) Listener accept: si el fd es el listener y hay POLLIN => accept()
		if (curFd == ft.socketFileTransfer && (_fds[i].revents & POLLIN))
		{
			struct sockaddr_in peerAddr;
			socklen_t alen = sizeof(peerAddr);
			int newfd = accept(ft.socketFileTransfer, (struct sockaddr*)&peerAddr, &alen);
			if (newfd != -1)
			{
				if (Server::setNonblock(newfd) == -1)
				{
					close(newfd);
					newfd = -1;
				}
				else
				{
					// IMPORTANTE:
					// este listener es para el receptor, no para el sender.
					// El sender ya se conecta por el socket activo que creamos con connect().
					if (ft.receiverFdRedDDC == -1)
					{
						ft.receiverFdRedDDC = newfd;
						ft.receiverClosed = false;
					}
					else
					{
						// ya hay un receptor conectado -> cerrar el excedente
						close(newfd);
						newfd = -1;
					}

					// si hemos aceptado correctamente, añadir newfd a poll y mapping
					if (newfd != -1)
					{
						pollfd p;
						p.fd = newfd;
						p.events = POLLIN;
						p.revents = 0;
						_fds.push_back(p);
						_fdToTransferId[newfd] = tid;
					}

					// si ahora tenemos ambos extremos, marcar bothConnected y notificar
					if (ft.senderFdRedDDC != -1 && ft.receiverFdRedDDC != -1)
					{
						ft.bothConnected = true;
						ft.lastActivity = std::time(NULL);

						// Intentar vaciar lo que ya hubiese llegado del sender antes de que el receptor entrase
						if (!ft.buf_peer_to_remote.empty())
						{
							if (!flushBufferToFd(_fds, ft, ft.buf_peer_to_remote, ft.receiverFdRedDDC, true))
								ft.closeAll();
						}

						// opcional: notificar al sender si está conectado
						if (_clients.find(ft.senderFd) != _clients.end())
						{
							std::string sNick = _clients[ft.senderFd].nickname;
							if (sNick.empty())
								sNick = intToString(ft.senderFd);
							sendNumeric(ft.senderFd, ":ircserv NOTICE " + sNick + " :DCC proxy connection established for id=" + intToString((int)ft.id));
						}
					}
				}
			}
			++i;
			return (true);
		}

		// 2) Completado de conexión no bloqueante del sender
		if (curFd == ft.senderFdRedDDC && (_fds[i].revents & POLLOUT))
		{
			int err = 0;
			socklen_t errlen = sizeof(err);
			if (getsockopt(curFd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1 || err != 0)
			{
				std::cerr << "ERROR: ft id=" << ft.id << " sender connect failed on fd=" << curFd << " errno=" << err << " (" << strerror(err) << ")\n";
				ft.closeAll();
				++i;
				return (true);
			}

			// Ya quedó conectado: dejamos de vigilar POLLOUT y pasamos a POLLIN
			setPollEvents(_fds, curFd, POLLIN);

			if (ft.receiverFdRedDDC != -1)
				ft.bothConnected = true;
			else
				ft.bothConnected = false;

			// Si ya había datos esperando del sender y el receptor ya está listo, intentamos enviarlos
			if (ft.receiverFdRedDDC != -1 && !ft.buf_peer_to_remote.empty())
			{
				if (!flushBufferToFd(_fds, ft, ft.buf_peer_to_remote, ft.receiverFdRedDDC, true))
					ft.closeAll();
			}

			++i;
			return (true);
		}

		// 3) Relay: lectura en peer o remote
		bool isPeer;
		if (curFd == ft.senderFdRedDDC)
			isPeer = true;
		else
			isPeer = false;

		bool isRemote;
		if (curFd == ft.receiverFdRedDDC)
			isRemote = true;
		else
			isRemote = false;

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

				// intentar enviar inmediatamente al otro extremo
				int dst;
				if (isPeer)
					dst = ft.receiverFdRedDDC;
				else
					dst = ft.senderFdRedDDC;

				std::string *outbuf;
				if (isPeer)
					outbuf = &ft.buf_peer_to_remote;
				else
					outbuf = &ft.buf_remote_to_peer;

				if (dst != -1)
				{
					// Los datos del sender son los que representan el archivo real,
					// por eso solo contamos bytesTransferred en esa dirección.
					bool countBytes = false;
					if (isPeer)
						countBytes = true;
					else
						countBytes = false;

					if (!flushBufferToFd(_fds, ft, *outbuf, dst, countBytes))
						ft.closeAll();
				}
			}
			else if (rn == 0)
			{
				if (isPeer)
				{
					// El sender cerró: cerramos solo ese socket y esperamos a dranar el buffer pendiente
					if (ft.senderFdRedDDC != -1)
					{
						_fdToTransferId.erase(ft.senderFdRedDDC);
						removePollFd(_fds, ft.senderFdRedDDC);
						close(ft.senderFdRedDDC);
						ft.senderFdRedDDC = -1;
					}
					ft.senderClosed = true;
				}
				else
				{
					// Si el receptor cierra, no tiene sentido seguir con la transferencia
					// porque ya no hay destino real para el archivo.
					ft.receiverClosed = true;
					ft.closeAll();
				}
			}
			else
			{
				// error no bloqueante: si no es EAGAIN/EWOULDBLOCK/EINTR -> cerrar
				if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
				{
					std::cerr << "ERROR: ft id=" << ft.id << " recv error errno=" << errno << " (" << strerror(errno) << ")\n";
					ft.closeAll();
				}
			}
			++i;
			return (true);
		}

		// 4) POLLOUT: intentar vaciar buffers si hay POLLOUT para este fd
		if ((isPeer || isRemote) && (_fds[i].revents & POLLOUT))
		{
			// Nota: POLLOUT en 'peer' indica que intentamos enviar hacia peer
			std::string *outbuf;
			bool countBytes = false;

			if (isPeer)
			{
				outbuf = &ft.buf_remote_to_peer;
				countBytes = false;
			}
			else
			{
				outbuf = &ft.buf_peer_to_remote;
				countBytes = true;
			}

			if (!flushBufferToFd(_fds, ft, *outbuf, curFd, countBytes))
				ft.closeAll();

			// Si ya vaciamos el buffer, quitar POLLOUT de events
			if (outbuf->empty())
				setPollEvents(_fds, curFd, POLLIN);

			++i;
			return (true);
		}

		// 5) Si la transferencia ya no está activa -> limpiar mappings y entradas en _fds
		if (!ft.isActive())
		{
			// borrar fds asociados del vector _fds y del map _fdToTransferId
			size_t k = 0;
			while (k < _fds.size())
			{
				int fdk = _fds[k].fd;
				if (fdk == ft.socketFileTransfer || fdk == ft.senderFdRedDDC || fdk == ft.receiverFdRedDDC)
				{
					_fdToTransferId.erase(fdk);
					// cerrar por seguridad si queda abierto (comprobar fd válido)
					if (fdk >= 0)
						close(fdk);
					_fds.erase(_fds.begin() + k);
					continue; // no incrementar k
				}
				++k;
			}
			_transfers.erase(tid);
			++i;
			return (true);
		}

		// si llegamos aquí no era un evento relevante para la transferencia (seguir con el flujo normal)
	}
	return (false);
}

/* Limpia todas las transfers que referencien al cliente badfd.
   Esta lógica estaba duplicada varias veces en runLoop; la centralizamos aquí.
   NOTA: conserva exactamente los mismos pasos que antes: closeAll(), borrar mappings,
   borrar fds del vector _fds, y eliminar las entradas de _transfers.
*/
void Server::cleanupTransfersForClient(int badfd)
{
	// Limpiar transfers que referencien este cliente como sender/receiver
	std::vector<unsigned long> toEraseTids;
	std::map<unsigned long, FileTransfer>::iterator ittr = _transfers.begin();
	while (ittr != _transfers.end())
	{
		unsigned long candTid = ittr->first;
		FileTransfer &cand = ittr->second;
		if (cand.senderFd == badfd || cand.receiverFd == badfd)
		{
			// capturar fds previo al closeAll
			int lfd = cand.socketFileTransfer;
			int pfd = cand.senderFdRedDDC;
			int rfd = cand.receiverFdRedDDC;

			// cerrar recursos
			cand.closeAll();

			// borrar mappings por seguridad
			if (lfd != -1) 
				_fdToTransferId.erase(lfd);
			if (pfd != -1)
				_fdToTransferId.erase(pfd);
			if (rfd != -1)
				_fdToTransferId.erase(rfd);

			// quitar fds de _fds si estaban presentes
			size_t kk = 0;
			while (kk < _fds.size())
			{
				int fdk = _fds[kk].fd;
				if (fdk == lfd || fdk == pfd || fdk == rfd)
				{
					// cerrar por si acaso
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

	// borrar transfers recogidos
	size_t x = 0;
	while (x < toEraseTids.size())
	{
		_transfers.erase(toEraseTids[x]);
		++x;
	}
}


