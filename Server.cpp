/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:20 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/16 11:58:43 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

bool Server::nickInUse(const std::string &nick) const
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

int Server::getFdByNick(const std::string &nick) const
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

// helper para enviar mensajes numéricos o líneas
void Server::sendNumeric(int fd, const std::string &msg)
{
	// Si el fd no está en la tabla de clientes, no hacemos nada.
	if (_clients.find(fd) == _clients.end())
		return;

	// Añadimos CRLF según protocolo
	_clients[fd].outbuf += msg + "\r\n";

	// marcar POLLOUT: buscar en _fds y añadir POLLOUT a ese fd
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
		return;
}

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
	// Si tenemos un server_fd abierto, lo cerramos porque vamos a sobrescribir el objeto.
	if (_server_fd != -1)
	{
		close(_server_fd);
		_server_fd = -1;
	}
	// cerramos todos los fds monitorizados (si los hay)
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

/*esta funcion hace que el socket no sea blocante, es decir que el primer cliente
bloquearia a los siguientes si no envia nada y los siguientes se quieren conectar o mandar algo*/
int Server::setNonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return (-1);
	return (fcntl(fd, F_SETFL, flags | O_NONBLOCK));
}

bool Server::initAndListen(long port, const std::string &password)
{
	_serverPassword = password;

	// Crear socket servidor que es el que escucha
	_server_fd = socket(AF_INET, SOCK_STREAM, 0); //AF_INET=IPV4, SOCK_STREAM=TCP, 0=protocolo por defecto
	if (_server_fd == -1)
	{
		std::cerr << "ERROR: socket failure\n";
		return (false);
	}

	std::cout << "Socket creado correctamente\n";

	/*es para reusar el mismo puerto porque el kernel cuando cierras pone el puerto en time_wait y sin ello
	no podrias usar el mismo puerto si cerramos el irc y lo volvemos a ejecutar acto seguido*/
	int opt = 1;
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		/*server_fd=socket, SOL_SOCKET=aplica a todo el socket(no solo al puerto o ip o protocolo),
		SO_REUSEADDR=permite reusar ip y puerto, &opt=activarlo, sizeof(opt)=tamaño valor*/
		std::cerr << "WARNING: setsockopt falló\n";
	}

	// Hacer socket no bloqueante
	if (Server::setNonblock(_server_fd) == -1)
		std::cerr << "WARNING: no se pudo poner server_fd non-blocking\n";

	// Dirección servidor
	sockaddr_in server_addr; //estructura que ya existe para guardar ip y puerto del servidor
	std::memset(&server_addr, 0, sizeof(server_addr)); //seteamos a 0 toda la estructura
	server_addr.sin_family = AF_INET; //IPv4
	server_addr.sin_port = htons((uint16_t)port); //Conversion del numero del puerto en bits para que todos los ordenadores lo interpreten igual
	server_addr.sin_addr.s_addr = INADDR_ANY; //cualquier adapator de red del ordenador, ethernet, wifi, localhost,...

	if (bind(_server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) //le decimos a que puerto y ip esta asociado el socket
	{
		std::cerr << "ERROR: bind failure\n";
		close(_server_fd);
		_server_fd = -1;
		return (false);
	}

	std::cout << "Bind hecho correctamente\n";

	if (listen(_server_fd, SOMAXCONN) == -1)
	{
		/*convierte el socket en un servidor que escucha todo lo que le manden los clientes*/
		/*SOMAXCONN = valor del sistema que representa la máxima cantidad de conexiones pendientes permitidas*/
		std::cerr << "ERROR: listen failure\n";
		close(_server_fd);
		_server_fd = -1;
		return (false);
	}

	std::cout << "Servidor escuchando...\n";

	// Estructura poll
	pollfd pfd;
	pfd.fd = _server_fd; //vigilar el servidor
	pfd.events = POLLIN; //avisame cuando haya datos a leer
	pfd.revents = 0; //eventos que han pasado
	_fds.push_back(pfd);

	return (true);
}

int Server::runLoop()
{
	while (g_running)
	{
		int ret = poll(&_fds[0], _fds.size(), -1); // espera que haya algun evento en un cliente o en el servidor
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "ERROR: poll falló\n";
			break;
		}
		size_t i = 0;
		while (i < _fds.size())
		{
			// Si no hay eventos para este fd, seguimos
			if (_fds[i].revents == 0)
			{
				++i;
				continue;
			}

			/* --- Manejo de fds asociados a FileTransfer ---
			   Este bloque prioriza eventos de transferencias de archivos (listeners, peer, remote).
			   Si el fd actual pertenece a una transferencia, lo manejamos aquí y continuamos.
			*/
			{
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
						continue;
					}
					FileTransfer &ft = itft->second;

					// 1) Listener accept: si el fd es el listener y hay POLLIN => accept()
					if (curFd == ft.listenerFd && (_fds[i].revents & POLLIN))
					{
						struct sockaddr_in peerAddr;
						socklen_t alen = sizeof(peerAddr);
						int newfd = accept(ft.listenerFd, (struct sockaddr*)&peerAddr, &alen);
						if (newfd != -1)
						{
							if (Server::setNonblock(newfd) == -1)
							{
								close(newfd);
								newfd = -1;
							}
							else
							{
								// asignar al primer slot libre (peerFd o remoteFd)
								if (ft.peerFd == -1)
									ft.peerFd = newfd;
								else if (ft.remoteFd == -1)
									ft.remoteFd = newfd;
								else
								{
									// ya hay dos extremos ocupados -> cerrar el excedente
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
								if (ft.peerFd != -1 && ft.remoteFd != -1)
								{
									ft.bothConnected = true;
									ft.lastActivity = std::time(NULL);
									// opcional: notificar al sender si está conectado
									if (_clients.find(ft.senderFd) != _clients.end())
									{
										std::string sNick = _clients[ft.senderFd].nickname;
										if (sNick.empty()) sNick = intToString(ft.senderFd);
										sendNumeric(ft.senderFd, ":ircserv NOTICE " + sNick + " :DCC proxy connection established for id=" + intToString((int)ft.id));
									}
								}
							}
						}
						++i;
						continue;
					}

					// 2) Relay: lectura en peer o remote
					bool isPeer = (curFd == ft.peerFd);
					bool isRemote = (curFd == ft.remoteFd);
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
								dst = ft.remoteFd;
							else
								dst = ft.peerFd;

							std::string *outbuf;
							if (isPeer)
								outbuf = &ft.buf_peer_to_remote;
							else
								outbuf = &ft.buf_remote_to_peer;

							if (dst != -1)
							{
								while (!outbuf->empty())
								{
									ssize_t sent = send(dst, outbuf->c_str(), outbuf->size(), 0);
									if (sent > 0)
										outbuf->erase(0, sent);
									else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
									{
										// no podemos escribir ahora, asegurarnos de vigilar POLLOUT del dst
										size_t j = 0;
										while (j < _fds.size())
										{
											if (_fds[j].fd == dst)
											{
												_fds[j].events |= POLLOUT;
												break;
											}
											++j;
										}
										break;
									}
									else
									{
										// error en send -> cerrar transferencia
										ft.closeAll();
										break;
									}
								}
							}
						}
						else if (rn == 0)
						{
							// EOF: cerrar transferencia
							ft.closeAll();
						}
						else
						{
							// error no bloqueante: si no es EAGAIN/EWOULDBLOCK/EINTR -> cerrar
							if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
								ft.closeAll();
						}
						++i;
						continue;
					}

					// 3) POLLOUT: intentar vaciar buffers si hay POLLOUT para este fd
					if ((isPeer || isRemote) && (_fds[i].revents & POLLOUT))
					{
						// Nota: POLLOUT en 'peer' indica que intentamos enviar hacia peer
						std::string *outbuf;
						if (isPeer)
							outbuf = &ft.buf_remote_to_peer;
						else
							outbuf = &ft.buf_peer_to_remote;

						while (!outbuf->empty())
						{
							ssize_t sent = send(curFd, outbuf->c_str(), outbuf->size(), 0);
							if (sent > 0)
								outbuf->erase(0, sent);
							else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
								break;
							else
							{
								ft.closeAll();
								break;
							}
						}
						// Si ya vaciamos el buffer, quitar POLLOUT de events
						if (outbuf->empty())
							_fds[i].events &= ~POLLOUT;
						++i;
						continue;
					}

					// 4) Si la transferencia ya no está activa -> limpiar mappings y entradas en _fds
					if (!ft.isActive())
					{
						// borrar fds asociados del vector _fds y del map _fdToTransferId
						size_t k = 0;
						while (k < _fds.size())
						{
							int fdk = _fds[k].fd;
							if (fdk == ft.listenerFd || fdk == ft.peerFd || fdk == ft.remoteFd)
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
						continue;
					}

					// si llegamos aquí no era un evento relevante para la transferencia (seguir con el flujo normal)
				}
			}

			/*Error o desconexión en un cliente o en el servidor 
			POLLERR=conexion rota, POLLHUP=cierras la terminal, POLLNVAL=fd corrupto*/
			if (_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				// Si el fd desconectado tiene asociado algun transfer (mapping), y además es cliente "normal",
				// debemos limpiar las transferencias relacionadas (sender o receiver)
				int badfd = _fds[i].fd;

				// Si el fd es server_fd -> error crítico
				if (badfd == _server_fd)
				{
					std::cerr << "ERROR en server_fd (poll)\n";
					g_running = 0;
					break;
				}

				// Antes de cerrar, limpiar transfers que referencien este cliente como sender/receiver
				std::vector<unsigned long> toEraseTids;
				std::map<unsigned long, FileTransfer>::iterator ittr = _transfers.begin();
				while (ittr != _transfers.end())
				{
					unsigned long candTid = ittr->first;
					FileTransfer &cand = ittr->second;
					if (cand.senderFd == badfd || cand.receiverFd == badfd)
					{
						// capturar fds previo al closeAll
						int lfd = cand.listenerFd;
						int pfd = cand.peerFd;
						int rfd = cand.remoteFd;

						// cerrar recursos
						cand.closeAll();

						// borrar mappings por seguridad
						if (lfd != -1) _fdToTransferId.erase(lfd);
						if (pfd != -1) _fdToTransferId.erase(pfd);
						if (rfd != -1) _fdToTransferId.erase(rfd);

						// quitar fds de _fds si estaban presentes
						size_t kk = 0;
						while (kk < _fds.size())
						{
							int fdk = _fds[kk].fd;
							if (fdk == lfd || fdk == pfd || fdk == rfd)
							{
								// cerrar por si acaso
								if (fdk >= 0) close(fdk);
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
				for (size_t x = 0; x < toEraseTids.size(); ++x)
					_transfers.erase(toEraseTids[x]);

				// Ahora cerramos y borramos el cliente (como antes)
				std::cout << "Cliente (fd " << badfd << ") se desconectó/err\n";
				close(badfd);
				_clients.erase(badfd);
				_fdToTransferId.erase(badfd); // por si acaso
				_fds.erase(_fds.begin() + i);
				// no incrementar i (ya apuntamos al siguiente elemento tras erase)
				continue;
			}

			// Nueva conexión entrante, POLLIN=poll recibe datos a leer
			if (_fds[i].fd == _server_fd && (_fds[i].revents & POLLIN))
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
						std::cerr << "ERROR en accept nuevo cliente\n";
						break;
					}

					if (Server::setNonblock(client_fd) == -1)
					{
						std::cerr << "WARNING: no se pudo poner client_fd non-blocking\n";
						close(client_fd);
						break;
					}

					pollfd cp;
					cp.fd = client_fd;
					cp.events = POLLIN;
					cp.revents = 0;
					_fds.push_back(cp);

					_clients[client_fd] = Client(client_fd);

					char client_ip[INET_ADDRSTRLEN]; //tamaño máximo para almacenar la representación en texto de una dirección IPv4
					if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL)
						std::cout << "Cliente conectado (fd " << client_fd << ") desde [ip desconocida]:" << ntohs(client_addr.sin_port) << "!\n";
					else
						std::cout << "Cliente conectado desde " << client_ip << ":" << ntohs(client_addr.sin_port) << " (fd " << client_fd << ")\n";
				}
				++i;
				continue;
			}

			// Lectura normal en cliente (POLLIN)
			if (_fds[i].revents & POLLIN)
			{
				int clientFd = _fds[i].fd;
				ssize_t n = recv(clientFd, _buf, BUF_SIZE, 0); //la cantidad de bytes que ha enviado el cliente al servidor

				if (n > 0)
				{
					_clients[clientFd].accum.append(_buf, _buf + n);

					size_t pos;
					while ((pos = _clients[clientFd].accum.find('\n')) != std::string::npos)
					{
						std::string line = _clients[clientFd].accum.substr(0, pos);
						if (!line.empty() && line[line.size() - 1] == '\r')
							line.erase(line.size() - 1);

						std::cout << "fd " << clientFd << " -> Línea recibida: [" << line << "]\n";

						/* ================= FASE DE AUTENTICACIÓN ================= */
						/* Ahora el orden no importa.
							Permitimos recibir PASS, NICK, USER en cualquier orden.
							El cliente solo queda registrado cuando tiene:
							- PASS correcto
							- NICK
							- USER
						*/
						if (!_clients[clientFd].registered)
						{
							// Siempre permitir PASS y CAP antes del registro
							handleInitialAuthentication(i, line);

							/* ---------------------------------------------------------
							Cliente NO registrado aún.
							Procesamos NICK y USER en cualquier orden.
							--------------------------------------------------------- */

							if (line.compare(0, 5, "NICK ") == 0)
								handleNickCommand(clientFd, line);
							else if (line.compare(0, 5, "USER ") == 0)
								handleUserCommand(clientFd, line);
							else if (line.compare(0, 4, "JOIN") == 0 || line.compare(0, 7, "PRIVMSG") == 0 ||
									line.compare(0, 4, "KICK") == 0 || line.compare(0, 6, "INVITE") == 0 ||
									line.compare(0, 5, "TOPIC") == 0 || line.compare(0, 4, "MODE") == 0 ||
									line.compare(0, 4, "PART") == 0)
							{

								Client &client = _clients[clientFd];

								std::cout << "fd " << clientFd << " intentó usar [" << line << "] sin registrarse. Falta: ";
								if (!client.correctPass)
									std::cout << "PASS ";
								if (client.nickname.empty())
									std::cout << "NICK ";
								if (client.username.empty())
									std::cout << "USER ";
								std::cout << std::endl;

								// ERR_NOTREGISTERED 451
								sendNumeric(clientFd, "451 :You have not registered");
							}

							// Si ya tenemos PASS correcto + NICK + USER → registrar
							if (_clients[clientFd].correctPass && !_clients[clientFd].nickname.empty() && !_clients[clientFd].username.empty())
							{
								_clients[clientFd].registered = true;

								// enviar RPL_WELCOME (001) -- formato simplificado
								std::string welcome = "001 " + _clients[clientFd].nickname + " :Welcome to the simple IRCd";

								sendNumeric(clientFd, welcome);

								std::cout << "fd " << clientFd << " registrado (PASS+NICK+USER). Enviada 001.\n";
							}
						}
						else
						{
							/* como irssi manda CAP para imprimir mensaje en el servidor de que lo ignoramos*/
							if (line.compare(0, 4, "CAP ") == 0)
								handleInitialAuthentication(i, line);
							/* IRSSI(cliente) esta mandando PING y si no contestamos PONG IRSSI(cliente) se desconecta*/
							else if (line.compare(0, 5, "PING ") == 0)
							{
								std::string ping_target = line.substr(5);
								sendNumeric(clientFd, "PONG " + ping_target);
								std::cout << "fd " << clientFd << " -> Respondido PONG a [" << ping_target << "]\n";
							}
							/*JOIN = para conectarte a un canal, los canales se llaman con prefijos: # ! & +*/
							else if (line.compare(0, 5, "JOIN ") == 0)
								handleJoinCommand(clientFd, line);
							/*PART = salir de un canal*/
							else if (line.compare(0, 5, "PART ") == 0)
								handlePartCommand(clientFd, line);
							/*PRIVMSG = mensaje privado*/
							else if (line.compare(0, 8, "PRIVMSG ") == 0)
								handlePrivmsgCommand(clientFd, line);
							/*KICK = operator expulsa a un regular user de un caanal*/
							else if (line.compare(0, 5, "KICK ") == 0)
								handleKickCommand(clientFd, line);
							/*INVITE = operator invita a un cliente a un canaal*/
							else if (line.compare(0, 7, "INVITE ") == 0)
								handleInviteCommand(clientFd, line);
							/*TOPIC = Ver el topic del canal o cambiarlo*/
							else if (line.compare(0, 6, "TOPIC ") == 0)
								handleTopicCommand(clientFd, line);
							/*MODE = operator puede cambiar diversas cosas con la flag +i +t +k +o +l*/
							else if (line.compare(0, 5, "MODE ") == 0)
								handleModeCommand(clientFd, line);
							/*una vez ya autorizado respondemos si nos ponen denuevo estos comandos de autorizacion*/
							else if (line.compare(0, 5, "PASS ") == 0 || line.compare(0, 5, "USER ") == 0)
							{
								// ERR_ALREADYREGISTERED 462
								std::string cur = _clients[clientFd].nickname;
								if (cur.empty())
									cur = "*";
								// Usamos prefijo de servidor como en otros mensajes de error
								sendNumeric(clientFd, ":ircserv 462 " + cur + " :You may not reregister");
							}
							/*NICK = cambiar el nickname una vez el cliente ya esta registrado*/
							else if (line.compare(0, 5, "NICK ") == 0)
							{
								// Guardamos el nick actual poder saber si cambia
								std::string originalNick = _clients[clientFd].nickname;

								// Construimos el prefijo antiguo nick!user@host
								std::string displayOldNick;
								if (originalNick.empty())
									displayOldNick = intToString(clientFd);
								else
									displayOldNick = originalNick;

								std::string user;
								if (_clients[clientFd].username.empty())
									user = "user";
								else
									user = _clients[clientFd].username;

								std::string oldPrefix = displayOldNick + "!" + user + "@localhost";

								// Reutilizamos la función que ya valida y asigna el nuevo nick
								handleNickCommand(clientFd, line);

								// Si el nick cambió correctamente notificamos a los demás clientes
								std::string newNick = _clients[clientFd].nickname;
								if (newNick != originalNick && !newNick.empty())
								{
									std::string out = ":" + oldPrefix + " NICK " + newNick;

									// Broadcast simple a todos los clientes conectados
									std::map<int, Client>::iterator it = _clients.begin();
									while (it != _clients.end())
									{
										sendNumeric(it->first, out);
										++it;
									}
								}
							}
							/*Comandos no implementados o desconocidgos*/
							else
								sendNumeric(clientFd, "421 :Unknown command");
						}
						_clients[clientFd].accum.erase(0, pos + 1);
					}
				}
				else if (n == 0)
				{
					// Cliente cerró conexión de forma ordenada -> limpiar también transfers relacionados
					int closedFd = clientFd;
					std::cout << "Cliente (fd " << closedFd << ") cerró conexión\n";

					// Limpiar transfers que referencien este cliente como sender/receiver
					std::vector<unsigned long> toEraseTids;
					std::map<unsigned long, FileTransfer>::iterator ittr2 = _transfers.begin();
					while (ittr2 != _transfers.end())
					{
						unsigned long candTid = ittr2->first;
						FileTransfer &cand = ittr2->second;
						if (cand.senderFd == closedFd || cand.receiverFd == closedFd)
						{
							int lfd = cand.listenerFd;
							int pfd = cand.peerFd;
							int rfd = cand.remoteFd;

							cand.closeAll();

							if (lfd != -1) _fdToTransferId.erase(lfd);
							if (pfd != -1) _fdToTransferId.erase(pfd);
							if (rfd != -1) _fdToTransferId.erase(rfd);

							// borrar esos fds del vector _fds
							size_t kk = 0;
							while (kk < _fds.size())
							{
								int fdk = _fds[kk].fd;
								if (fdk == lfd || fdk == pfd || fdk == rfd)
								{
									if (fdk >= 0) close(fdk);
									_fds.erase(_fds.begin() + kk);
									continue;
								}
								++kk;
							}

							toEraseTids.push_back(candTid);
						}
						++ittr2;
					}
					for (size_t x = 0; x < toEraseTids.size(); ++x)
						_transfers.erase(toEraseTids[x]);

					// ahora cerrar y borrar cliente
					close(closedFd);
					_clients.erase(closedFd);
					_fdToTransferId.erase(closedFd); // por si acaso estaba mapeado
					_fds.erase(_fds.begin() + i);
					continue;
				}
				else
				{
					if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
					{
						std::cerr << "ERROR: recv falló\n";
						int bad = clientFd;
						// limpiar transfers asociados igual que en EOF
						std::vector<unsigned long> toEraseTids;
						std::map<unsigned long, FileTransfer>::iterator ittr3 = _transfers.begin();
						while (ittr3 != _transfers.end())
						{
							unsigned long candTid = ittr3->first;
							FileTransfer &cand = ittr3->second;
							if (cand.senderFd == bad || cand.receiverFd == bad)
							{
								int lfd = cand.listenerFd;
								int pfd = cand.peerFd;
								int rfd = cand.remoteFd;

								cand.closeAll();

								if (lfd != -1) _fdToTransferId.erase(lfd);
								if (pfd != -1) _fdToTransferId.erase(pfd);
								if (rfd != -1) _fdToTransferId.erase(rfd);

								size_t kk = 0;
								while (kk < _fds.size())
								{
									int fdk = _fds[kk].fd;
									if (fdk == lfd || fdk == pfd || fdk == rfd)
									{
										if (fdk >= 0) close(fdk);
										_fds.erase(_fds.begin() + kk);
										continue;
									}
									++kk;
								}

								toEraseTids.push_back(candTid);
							}
							++ittr3;
						}
						for (size_t x = 0; x < toEraseTids.size(); ++x)
							_transfers.erase(toEraseTids[x]);

						// cerrar el cliente problemático
						close(clientFd);
						_clients.erase(clientFd);
						_fdToTransferId.erase(clientFd);
						_fds.erase(_fds.begin() + i);
						continue;
					}
				}
			}

			// Manejo de POLLOUT para sockets de cliente normales (enviar buffer de servidor->cliente)
			if (_fds[i].revents & POLLOUT)
			{
				int fd = _fds[i].fd;

				// Si por algún motivo el fd ya no existe en clients, lo cerramos
				if (_clients.find(fd) == _clients.end())
				{
					close(fd);
					_fds.erase(_fds.begin() + i);
					continue;
				}

				std::string &data = _clients[fd].outbuf;

				while (!data.empty())
				{
					ssize_t sent = send(fd, data.c_str(), data.size(), 0);

					if (sent > 0)
						data.erase(0, sent);
					else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
						break;
					else
					{
						std::cerr << "ERROR: send falló\n";
						// cerrar el cliente y limpiar transfers relacionados
						int badfd = fd;
						close(badfd);
						_clients.erase(badfd);
						_fdToTransferId.erase(badfd);

						// limpiar transfers relacionados (igual que antes)
						std::vector<unsigned long> toEraseTids;
						std::map<unsigned long, FileTransfer>::iterator ittr4 = _transfers.begin();
						while (ittr4 != _transfers.end())
						{
							unsigned long candTid = ittr4->first;
							FileTransfer &cand = ittr4->second;
							if (cand.senderFd == badfd || cand.receiverFd == badfd)
							{
								int lfd = cand.listenerFd;
								int pfd = cand.peerFd;
								int rfd = cand.remoteFd;

								cand.closeAll();

								if (lfd != -1) _fdToTransferId.erase(lfd);
								if (pfd != -1) _fdToTransferId.erase(pfd);
								if (rfd != -1) _fdToTransferId.erase(rfd);

								size_t kk = 0;
								while (kk < _fds.size())
								{
									int fdk = _fds[kk].fd;
									if (fdk == lfd || fdk == pfd || fdk == rfd)
									{
										if (fdk >= 0) close(fdk);
										_fds.erase(_fds.begin() + kk);
										continue;
									}
									++kk;
								}

								toEraseTids.push_back(candTid);
							}
							++ittr4;
						}
						for (size_t x = 0; x < toEraseTids.size(); ++x)
							_transfers.erase(toEraseTids[x]);

						// quitar entrada de _fds correspondiente al cliente (ya cerrada arriba)
						_fds.erase(_fds.begin() + i);
						continue;
					}
				}

				/*Si ya terminé de enviar todo al cliente, deja de preguntarle al sistema si puedo escribir*/
				if (_clients.find(fd) != _clients.end() && _clients[fd].outbuf.empty())
					_fds[i].events &= ~POLLOUT; /*esta linea=Deja de vigilar escritura para este socket y la ~ es para invertir todos los bits de POLLOUT*/
			}

			// avanzar al siguiente fd
			++i;
		}
	}
	/*Mensaje de que el servidor se ha cerrado con Ctr+c o kill o Ctr+\*/
	std::string shutdown_msg = "ERROR :Server is shutting down\r\n";
	std::map<int, Client>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		int fd = it->first;
		// Intentamos enviar de forma directa; si falla, lo ignoramos porque vamos a cerrar.
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
