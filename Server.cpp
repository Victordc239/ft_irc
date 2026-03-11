/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:20 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/11 19:41:35 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

bool Server::nickInUse(const std::string &nick) const
{
	std::map<int, Client>::const_iterator iterator = this->_clients.begin();

	while (iterator != this->_clients.end())
	{
		if (iterator->second.nickname == nick)
			return true;
		++iterator;
	}
	return false;
}

int Server::getFdByNick(const std::string &nick) const
{
	std::map<int, Client>::const_iterator iterator = this->_clients.begin();

	while (iterator != this->_clients.end())
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
	if (this->_clients.find(fd) == this->_clients.end())
	{
		std::cout << "DEBUG sendNumeric: fd " << fd << " no existe en _clients. Mensaje descartado: [" << msg << "]\n";
		return;
	}

	// Añadimos CRLF según protocolo
	this->_clients[fd].outbuf += msg + "\r\n";

	// marcar POLLOUT: buscar en _fds y añadir POLLOUT a ese fd
	bool found = false;
	size_t j = 0;

	while (j < this->_fds.size())
	{
		if (this->_fds[j].fd == fd)
		{
			this->_fds[j].events |= POLLOUT;
			found = true;
			std::cout << "DEBUG sendNumeric: marcado POLLOUT para fd " << fd << " (msg: [" << msg << "])\n";
			break;
		}
		++j;
	}

	if (!found)
		std::cout << "DEBUG sendNumeric: no existe pollfd para fd " << fd << " (msg: [" << msg << "])\n";
}

Server::Server()
{
	this->_server_fd = -1;
	this->_serverPassword = "";
	std::memset(this->_buf, 0, sizeof(this->_buf));
}

Server::Server(const Server &other)
{
	this->_server_fd = -1;
	this->_serverPassword = other._serverPassword;
	std::memcpy(this->_buf, other._buf, sizeof(this->_buf));
	this->_fds.clear();
	this->_clients.clear();
}

Server &Server::operator=(const Server &other)
{
	if (this == &other)
		return *this;
	// Si tenemos un server_fd abierto, lo cerramos porque vamos a sobrescribir el objeto.
	if (this->_server_fd != -1)
	{
		close(this->_server_fd);
		this->_server_fd = -1;
	}
	// cerramos todos los fds monitorizados (si los hay)
	size_t j = 0;
	while (j < this->_fds.size())
	{
		close(this->_fds[j].fd);
		++j;
	}
	this->_fds.clear();
	this->_clients.clear();
	this->_serverPassword = other._serverPassword;
	std::memcpy(this->_buf, other._buf, sizeof(this->_buf));
	return *this;
}

Server::~Server()
{
	if (this->_server_fd != -1)
		close(this->_server_fd);

	size_t j = 0;
	while (j < this->_fds.size())
	{
		close(this->_fds[j].fd);
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
	this->_serverPassword = password;

	// Crear socket servidor que es el que escucha
	this->_server_fd = socket(AF_INET, SOCK_STREAM, 0); //AF_INET=IPV4, SOCK_STREAM=TCP, 0=protocolo por defecto
	if (this->_server_fd == -1)
	{
		std::cerr << "ERROR: socket failure\n";
		return false;
	}

	std::cout << "Socket creado correctamente\n";

	/*es para reusar el mismo puerto porque el kernel cuando cierras pone el puerto en time_wait y sin ello
	no podrias usar el mismo puerto si cerramos el irc y lo volvemos a ejecutar acto seguido*/
	int opt = 1;
	if (setsockopt(this->_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		/*server_fd=socket, SOL_SOCKET=aplica a todo el socket(no solo al puerto o ip o protocolo),
		SO_REUSEADDR=permite reusar ip y puerto, &opt=activarlo, sizeof(opt)=tamaño valor*/
		std::cerr << "WARNING: setsockopt falló\n";
	}

	// Hacer socket no bloqueante
	if (Server::setNonblock(this->_server_fd) == -1)
		std::cerr << "WARNING: no se pudo poner server_fd non-blocking\n";

	// Dirección servidor
	sockaddr_in server_addr; //estructura que ya existe para guardar ip y puerto del servidor
	std::memset(&server_addr, 0, sizeof(server_addr)); //seteamos a 0 toda la estructura
	server_addr.sin_family = AF_INET; //IPv4
	server_addr.sin_port = htons((uint16_t)port); //Conversion del numero del puerto en bits para que todos los ordenadores lo interpreten igual
	server_addr.sin_addr.s_addr = INADDR_ANY; //cualquier adapator de red del ordenador, ethernet, wifi, localhost,...

	if (bind(this->_server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) //le decimos a que puerto y ip esta asociado el socket
	{
		std::cerr << "ERROR: bind failure\n";
		close(this->_server_fd);
		this->_server_fd = -1;
		return false;
	}

	std::cout << "Bind hecho correctamente\n";

	if (listen(this->_server_fd, SOMAXCONN) == -1)
	{
		/*convierte el socket en un servidor que escucha todo lo que le manden los clientes*/
		/*SOMAXCONN = valor del sistema que representa la máxima cantidad de conexiones pendientes permitidas*/
		std::cerr << "ERROR: listen failure\n";
		close(this->_server_fd);
		this->_server_fd = -1;
		return false;
	}

	std::cout << "Servidor escuchando...\n";

	// Estructura poll
	pollfd pfd;
	pfd.fd = this->_server_fd; //vigilar el servidor
	pfd.events = POLLIN; //avisame cuando haya datos a leer
	pfd.revents = 0; //eventos que han pasado
	this->_fds.push_back(pfd);

	return true;
}

int Server::runLoop()
{
	while (g_running)
	{
		int ret = poll(&this->_fds[0], this->_fds.size(), -1); // espera que haya algun evento en un cliente o en el servidor
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "ERROR: poll falló\n";
			break;
		}

		size_t i = 0;
		while (i < this->_fds.size())
		{
			if (this->_fds[i].revents == 0)
			{
				++i;
				continue;
			}

			/*Error o desconexión en un cliente o en el servidor 
			POLLERR=conexion rota, POLLHUP=cierras la terminal, POLLNVAL=fd corrupto*/
			if (this->_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (this->_fds[i].fd == this->_server_fd)
				{
					std::cerr << "ERROR en server_fd (poll)\n";
					g_running = 0;
					break;
				}
				else
				{
					std::cout << "Cliente (fd " << this->_fds[i].fd << ") se desconectó/err\n";
					close(this->_fds[i].fd);
					this->_clients.erase(this->_fds[i].fd);
					this->_fds.erase(this->_fds.begin() + i);
					continue;
				}
			}

			// Nueva conexión entrante, POLLIN=poll recibe datos a leer
			if (this->_fds[i].fd == this->_server_fd && (this->_fds[i].revents & POLLIN))
			{
				while (true)
				{
					sockaddr_in client_addr;
					socklen_t client_len = sizeof(client_addr);
					int client_fd = accept(this->_server_fd, (sockaddr*)&client_addr, &client_len);
					if (client_fd == -1)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;
						std::cerr << "ERROR en accept nuevo cliente\n";
						break;
					}

					if (Server::setNonblock(client_fd) == -1)
						std::cerr << "WARNING: no se pudo poner client_fd non-blocking\n";

					pollfd cp;
					cp.fd = client_fd;
					cp.events = POLLIN;
					cp.revents = 0;
					this->_fds.push_back(cp);

					this->_clients[client_fd] = Client(client_fd);

					char client_ip[INET_ADDRSTRLEN]; //tamaño máximo para almacenar la representación en texto de una dirección IPv4
					if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL)
						std::cout << "Cliente conectado (fd " << client_fd << ") desde [ip desconocida]:" << ntohs(client_addr.sin_port) << "!\n";
					else
						std::cout << "Cliente conectado desde " << client_ip << ":" << ntohs(client_addr.sin_port) << " (fd " << client_fd << ")\n";
				}
				++i;
				continue;
			}

			if (this->_fds[i].revents & POLLIN)
			{
				int clientFd = this->_fds[i].fd;
				ssize_t n = recv(clientFd, this->_buf, BUF_SIZE, 0); //la cantidad de bytes que ha enviado el cliente al servidor

				if (n > 0)
				{
					this->_clients[clientFd].accum.append(this->_buf, this->_buf + n);

					size_t pos;
					while ((pos = this->_clients[clientFd].accum.find('\n')) != std::string::npos)
					{
						std::string line = this->_clients[clientFd].accum.substr(0, pos);
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
						if (!this->_clients[clientFd].registered)
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
								// ERR_NOTREGISTERED 451
								sendNumeric(clientFd, "451 :You have not registered");
							}

							// Si ya tenemos PASS correcto + NICK + USER → registrar
							if (this->_clients[clientFd].correctPass && !this->_clients[clientFd].nickname.empty() && !this->_clients[clientFd].username.empty())
							{
								this->_clients[clientFd].registered = true;

								// enviar RPL_WELCOME (001) -- formato simplificado
								std::string welcome = "001 " + this->_clients[clientFd].nickname + " :Welcome to the simple IRCd";

								sendNumeric(clientFd, welcome);

								std::cout << "fd " << clientFd << " registrado (PASS+NICK+USER). Enviada 001.\n";
							}
						}
						else
						{
							/* Cliente ya autenticado y registrado: procesar comandos normales */
							/* IRSSI(cliente) esta mandando PING y si no contestamos PONG IRSSI(cliente) se desconecta*/
							if (line.compare(0, 5, "PING ") == 0)
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
							/*Comandos no implementados o desconocidgos*/
							else
								sendNumeric(clientFd, "421 :Unknown command");
						}
						this->_clients[clientFd].accum.erase(0, pos + 1);
					}
				}
				else if (n == 0)
				{
					std::cout << "Cliente (fd " << clientFd << ") cerró conexión\n";
					close(clientFd);
					this->_clients.erase(clientFd);
					this->_fds.erase(this->_fds.begin() + i);
					continue;
				}
				else
				{
					if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
					{
						std::cerr << "ERROR: recv falló\n";
						close(clientFd);
						this->_clients.erase(clientFd);
						this->_fds.erase(this->_fds.begin() + i);
						continue;
					}
				}
			}

			if (this->_fds[i].revents & POLLOUT)
			{
				int fd = this->_fds[i].fd;

				// Si por algún motivo el fd ya no existe en clients, lo cerramos
				if (this->_clients.find(fd) == this->_clients.end())
				{
					close(fd);
					this->_fds.erase(this->_fds.begin() + i);
					continue;
				}

				std::string &data = this->_clients[fd].outbuf;

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
						close(fd);
						this->_clients.erase(fd);
						this->_fds.erase(this->_fds.begin() + i);
						continue;
					}
				}

				/*Si ya terminé de enviar todo al cliente, deja de preguntarle al sistema si puedo escribir*/
				if (this->_clients.find(fd) != this->_clients.end() && this->_clients[fd].outbuf.empty())
					this->_fds[i].events &= ~POLLOUT; /*esta linea=Deja de vigilar escritura para este socket y la ~ es para invertir todos los bits de POLLOUT*/
			}

			++i;
		}
	}

	/*Mensaje de que el servidor se ha cerrado con Ctr+c o kill o Ctr+\*/
	std::string shutdown_msg = "ERROR :Server is shutting down\r\n";
	std::map<int, Client>::iterator it = this->_clients.begin();
	while (it != this->_clients.end())
	{
		int fd = it->first;
		// Intentamos enviar de forma directa; si falla, lo ignoramos porque vamos a cerrar.
		ssize_t s = send(fd, shutdown_msg.c_str(), shutdown_msg.size(), 0);
		(void)s;
		++it;
	}
	
	size_t j = 0;
	while (j < this->_fds.size())
	{
		close(this->_fds[j].fd);
		++j;
	}

	return 0;
}