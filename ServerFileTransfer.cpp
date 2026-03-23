/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerFileTransfer.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 14:14:47 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/23 17:48:37 by sofernan         ###   ########.fr       */
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
				ft.bytesTransferred = ft.bytesTransferred + (unsigned long)sent;
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
				if (Server::setNonBlocking(newfd) == -1)
				{
					close(newfd);
					newfd = -1;
				}
				else
				{
					// IMPORTANTE:
					// este listener es para el receptor, no para el sender.
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
							if (!flushBufferToFd(_fds, ft, ft.buf_peer_to_remote, ft.receiverFdRedDDC, true))
								ft.closeAll();
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

		// 2) Completado de conexión no bloqueante del sender
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
				ft.closeAll();
				++i;
				return (true);
			}

			setPollEvents(_fds, curFd, POLLIN);
			ft.bothConnected = (ft.receiverFdRedDDC != -1);

			if (ft.receiverFdRedDDC != -1 && !ft.buf_peer_to_remote.empty())
			{
				if (!flushBufferToFd(_fds, ft, ft.buf_peer_to_remote, ft.receiverFdRedDDC, true))
					ft.closeAll();
			}

			++i;
			return (true);
		}

		// 👉 Delegamos TODO lo demás a la auxiliar
		if (handleFileTransferEventAux(i, curFd, tid))
			return (true);
	}
	return (false);
}

bool Server::handleFileTransferEventAux(size_t &i, int curFd, unsigned long tid)
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

	// 3) Relay: lectura en peer o remote
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
				if (!flushBufferToFd(_fds, ft, *outBuffer, dst, countBytes))
					ft.closeAll();
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
				ft.closeAll();
			}
		}
		else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			ft.closeAll();
		++i;
		return (true);
	}

	// 4) POLLOUT
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

		if (!flushBufferToFd(_fds, ft, *outBuffer, curFd, countBytes))
			ft.closeAll();

		if (outBuffer->empty())
			setPollEvents(_fds, curFd, POLLIN);
		++i;
		return (true);
	}

	// 5) Cleanup
	if (!ft.isActive())
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

bool	parseDccIpToken(const std::string &tok, struct in_addr &out)
{
	// Caso 1: IP decimal estilo DCC (ej: 2130706433)
	char *endptr = NULL;
	unsigned long ip_dec = strtoul(tok.c_str(), &endptr, 10);
	if (!tok.empty() && *endptr == '\0')
	{
		out.s_addr = htonl((uint32_t)ip_dec);
		return (true);
	}

	// Caso 2: IP normal con puntos (ej: 127.0.0.1)
	out.s_addr = inet_addr(tok.c_str());
	if (out.s_addr == INADDR_NONE && tok != "255.255.255.255")
		return (false);

	return (true);
}

bool Server::handleDccSendInPrivmsgDccProxy(int clientFd, int dst_fd, const std::vector<std::string> &toks, const std::string &prefix, const std::string &target, const std::string &filename, unsigned long fsize, unsigned long transferId)
{
	// Intentar conectar de forma activa al sender ORIGINAL (si el CTCP incluía IP y PORT). Muchos clientes (el emisor) están en modo "listen" y
	// esperan que el receptor conecte a ellos. Si queremos recibir bytes en el proxy, debemos conectarnos al sender usando la IP/PORT que nos mandó.

	// tratar de recuperar IP y PORT originales del sender si estaban en toks
	if (toks.size() >= 3)
	{
		std::string orig_ip_tok = toks[1];
		std::string orig_port_tok = toks[2];
		// convertir IP: puede venir como decimal (host order) o dotted; soportamos ambos
		struct sockaddr_in sender_addr;
		std::memset(&sender_addr, 0, sizeof(sender_addr));
		sender_addr.sin_family = AF_INET;
		bool haveSenderAddr = false;

		struct in_addr ina;
		if (parseDccIpToken(orig_ip_tok, ina))
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
			// crear socket y conectarnos al sender (non-blocking)
			int s = socket(AF_INET, SOCK_STREAM, 0);
			if (s != -1)
			{
				fcntl(s, F_SETFL, O_NONBLOCK);

				int cres = connect(s, (struct sockaddr*)&sender_addr, sizeof(sender_addr));
				if (cres == 0) // conectado de inmediato
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
				else if (errno == EINPROGRESS || errno == EINTR) // conexión en progreso: vigilamos POLLOUT para saber cuándo termina
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

	// Obtener IP del servidor para enviar al receptor (si no se puede, usar 127.0.0.1)
	// NOTA: no usar getsockname(_server_fd) cuando _server_fd está ligado a INADDR_ANY,
	// porque devolvería 0.0.0.0. En su lugar intentamos averiguar la IP saliente
	// creando un socket UDP y "conectándolo" a IP pública (no se envía tráfico).
	std::string serverIp = "127.0.0.1";
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock != -1)
	{
		struct sockaddr_in remote;
		std::memset(&remote, 0, sizeof(remote));
		remote.sin_family = AF_INET;
		// conectar a una IP pública solo para conocer la interfaz saliente
		remote.sin_addr.s_addr = inet_addr("8.8.8.8");
		remote.sin_port = htons(53);
		// connect no envía paquetes en UDP, solo deja elegir interfaz
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

	// Convertir IP (dotted) a entero decimal que usa DCC (ntohl(inet_addr(ip)))
	uint32_t ip_net = inet_addr(serverIp.c_str()); // network order
	uint32_t ip_decimal = ntohl(ip_net);          // host order decimal

	// Construir el CTCP DCC SEND modificado para el receptor (le decimos que se conecte al servidor)
	unsigned short port = _transfers[transferId].getListenerPort();
	if (port == 0)
	{
		// fallback: puerto inválido
		sendNumericMessage(clientFd, ":ircserv NOTICE :DCC proxy internal error (listener port=0)");
		return (true);
	}

	// Formato clásico DCC: IP en decimal (host order) y puerto en decimal.
	std::string dccmsg = "\001DCC SEND " + filename + " " + convertIntToString((int)ip_decimal) + " " + convertIntToString((int)port) + " " + convertIntToString((int)fsize) + "\001";
	std::string outmsg = ":" + prefix + " PRIVMSG " + target + " :" + dccmsg;

	// Enviar al receptor el CTCP con IP/puerto del proxy
	sendNumericMessage(dst_fd, outmsg);

	// Informar al emisor (opcional, mensaje NOTICE)
	std::string senderNick = _clients[clientFd].nickname;
	if (senderNick.empty())
		senderNick = convertIntToString(clientFd);
	sendNumericMessage(clientFd, ":ircserv NOTICE " + senderNick + " :DCC proxy created id=" + convertIntToString((int)transferId));

	return (true);
}

bool Server::handleDccSendInPrivmsg(int clientFd, int dst_fd, const std::string &text, const std::string &prefix, const std::string &target)
{
	// Si el texto comienza con 0x01 (CTCP) y contiene "DCC SEND " en la posición 1, lo consideramos una petición DCC SEND que podemos interceptar.
	// (esta función asume que ya comprobaste que text[0] == '\001' y que text.find("DCC SEND ",1) == 1)

	const std::string dccPrefix = "DCC SEND ";
	size_t pos = 1; // precondición de llamada

	// Extraer la porción después de "DCC SEND "
	size_t start = pos + dccPrefix.size(); // inicio de los tokens: filename ip port size
	std::string rest = "";
	if (start < text.size())
		rest = text.substr(start);

	// quitar un posible \001 final
	if (!rest.empty() && rest[rest.size() - 1] == '\001')
		rest.erase(rest.size() - 1);

	// Tokenizar rest por espacios de forma manual:
	std::vector<std::string> toks;
	std::string s = rest;
	while (!s.empty())
	{
		// saltar espacios iniciales
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

	// Esperamos al menos el filename (toks[0]). ip/port/size son opcionales
	if (!toks.empty())
	{
		std::string filename = toks[0];
		unsigned long fsize = 0;
		// si nos dan size (habitualmente en toks[3]) intentamos parsearlo
		if (toks.size() >= 4)
		{
			char *endptr = NULL;
			fsize = (unsigned long)ft_strtol(toks[3].c_str(), &endptr);
			if (*endptr != '\0')
				fsize = 0; // parse error -> 0
		}

		// Construir la transferencia y crear listener (proxy)
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

		// Intentar crear el listener (ephemeral port)
		if (ft.createListener() != 0)
		{
			// fallo: avisar al emisor y reenviar el PRIVMSG original como fallback
			sendNumericMessage(clientFd, ":ircserv NOTICE :DCC proxy failed to create listener");
			std::string fallback = ":" + prefix + " PRIVMSG " + target + " :" + text;
			sendNumericMessage(dst_fd, fallback);
			return (true);
		}

		// Guardar la transferencia en el mapa del servidor (copiamos metadata).
		_transfers[ft.id] = ft;

		// transferir ownership del fd al objeto en el mapa
		_transfers[ft.id].socketFileTransfer = ft.socketFileTransfer;
		// evitar que el destructor del objeto local cierre el fd (liberar propiedad)
		ft.socketFileTransfer = -1;

		// ahora sí obtenemos el fd real
		int lfd = _transfers[ft.id].socketFileTransfer;
		if (lfd == -1)
		{
			// fallback seguro (no debería ocurrir si createListener() tuvo éxito)
			sendNumericMessage(clientFd, ":ircserv NOTICE :DCC proxy internal error (no listener fd)");
			return (true);
		}

		// Añadir listener al array de poll para que runServerLoop() lo gestione
		pollfd pfd;
		pfd.fd = lfd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_fds.push_back(pfd);

		// Mapear el fd del listener a la transferencia
		_fdToTransferId[lfd] = ft.id;

		bool result = handleDccSendInPrivmsgDccProxy(clientFd, dst_fd, toks, prefix, target, filename, fsize, ft.id);
		return (result);
	}

	// Si no había tokens (no filename) -> no hicimos nada especial
	return (false);
}

//  Limpia todas las transfers que referencien al cliente badfd.
//    Esta lógica estaba duplicada varias veces en runServerLoop; la centralizamos aquí.
//    NOTA: conserva exactamente los mismos pasos que antes: closeAll(), borrar mappings,
//    borrar fds del vector _fds, y eliminar las entradas de _transfers.
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
