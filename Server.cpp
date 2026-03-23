/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:20 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/23 18:21:29 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

// Comprueba si un nick ya lo está usando algún cliente
bool Server::isNickInUse(const std::string &nick) const
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

// Devuelve el fd (socket) de un cliente a partir de su nick
int Server::findFdByNick(const std::string &nick) const
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
/* void Server::sendNumericMessage(int fd, const std::string &msg)
{
	// Si no conocemos el cliente, ignorar
	if (_clients.find(fd) == _clients.end())
		return;

	// Construir la línea completa con CRLF y encolar
	std::string full = msg + "\r\n";
	std::string &out = _clients[fd].outBuffer;
	out = out + full;

	// Intentar enviar ahora mismo (non-blocking). Si se envía todo, no necesitamos POLLOUT.
	if (!out.empty())
	{
		ssize_t s = send(fd, out.c_str(), out.size(), 0);
		if (s > 0)
			out.erase(0, s);
		else if (s == -1 && (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
		{
			std::cerr << "ERROR sendNumericMessage: send() failed for fd " << fd << " errno=" << errno << " (" << strerror(errno) << ")\n";
			// Manejo minimal: cerrar cliente aquí como haces en other places
			close(fd);
			_clients.erase(fd);
			// quitar fd de _fds si quieres (opcional)
			size_t kk = 0;
			while (kk < _fds.size())
			{
				if (_fds[kk].fd == fd)
				{
					_fds.erase(_fds.begin() + kk);
					break;
				}
				++kk;
			}
			return;
		}
	}

	// Si aún queda algo por enviar, asegurarnos de monitorizar POLLOUT.
	if (!out.empty())
	{
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

		// Si no existe entrada en _fds para este fd (posible causa del bug), crearla.
		if (!found)
		{
			pollfd p;
			p.fd = fd;
			p.events = POLLIN | POLLOUT; // queremos leer y escribir
			p.revents = 0;
			_fds.push_back(p);
		}
	}
	else
	{
		// Si todo fue enviado, quitar POLLOUT si existe.
		size_t j = 0;
		while (j < _fds.size())
		{
			if (_fds[j].fd == fd)
			{
				_fds[j].events &= ~POLLOUT;
				break;
			}
			++j;
		}
	}
} */

// Guarda y prepara mensajes numéricos formateados para enviarlos del servidor al cliente 
void Server::sendNumericMessage(int fd, const std::string &msg)
{
	// Si no conocemos el cliente, ignorar
	if (_clients.find(fd) == _clients.end())
		return;

	// Construir la línea completa con CRLF y encolar
	std::string full = msg + "\r\n";
	std::string &out = _clients[fd].outBuffer;

	// --- Protección: no permitir crecimiento infinito del buffer ---
	// Si el nuevo tamaño supera 1048576, desconectamos el cliente para
	// evitar OOM y comportamiento indefinido bajo flood.
	if (out.size() + full.size() > 1048576)
	{
		std::cerr << "WARNING: client fd " << fd << " exceeded 1MB (" << (out.size() + full.size()) << " bytes). Disconnecting.\n";

		// Limpiar transferencias que referencien a este cliente (si aplica)
		cleanupTransfersForClient(fd);

		// Cerrar y eliminar cliente de estructuras
		close(fd);
		_clients.erase(fd);
		_fdToTransferId.erase(fd);

		// Quitar fd de la lista de poll
		removePollFd(_fds, fd);

		return;
	}

	// Encolar el mensaje (NO intentar send() aquí)
	out = out + full;

	// Si aún queda algo por enviar, asegurarnos de monitorizar POLLOUT.
	if (!out.empty())
	{
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

		// Si no existe entrada en _fds para este fd (posible causa del bug), crearla.
		if (!found)
		{
			pollfd p;
			p.fd = fd;
			p.events = POLLIN | POLLOUT; // queremos leer y escribir
			p.revents = 0;
			_fds.push_back(p);
		}
	}
	else
	{
		// Si todo fue enviado (caso raro porque acabamos de encolar), quitar POLLOUT si existe.
		size_t j = 0;
		while (j < _fds.size())
		{
			if (_fds[j].fd == fd)
			{
				_fds[j].events &= ~POLLOUT;
				break;
			}
			++j;
		}
	}
}

// Inicializa el servidor con valores por defecto
Server::Server()
{
	_server_fd = -1;
	_serverPassword = "";
	std::memset(_buf, 0, sizeof(_buf));
	_nextTransferId = 1;
}

// Crea una copia del servidor sin copiar conexiones activas
Server::Server(const Server &other)
{
	_server_fd = -1;
	_serverPassword = other._serverPassword;
	std::memcpy(_buf, other._buf, sizeof(_buf));
	_nextTransferId = 1;
	_fds.clear();
	_clients.clear();
}

// Copia un servidor en otro, cerrando antes conexiones existentes
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

// Cierra todos los sockets al destruir el servidor
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

// Esta función configura el socket para que no sea bloqueante.
// Por defecto, si un cliente no envía datos, podría bloquear a otros clientes que intentan conectarse
// o enviar mensajes. Con este modo, cada cliente puede enviar y recibir sin afectar a los demás.
int Server::setNonBlocking(int fd)
{
	// int flags = fcntl(fd, F_GETFL, 0);
	// if (flags == -1)
	// 	return (-1);
	// return (fcntl(fd, F_SETFL, flags | O_NONBLOCK));

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		return (-1);
	return (0);
}

// Crea el socket del servidor, lo configura y empieza a escuchar conexiones
bool Server::InitSocketAndListen(long port, const std::string &password)
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
		std::cerr << "ERROR: setsockopt falló\n";

		// limpiar y salir: cerrar server_fd y cualquier otro fd abierto en _fds/_clients
		if (_server_fd != -1)
		{
			close(_server_fd);
			_server_fd = -1;
		}
		// por seguridad cerrar cualquier fd que pudiera haberse registrado antes (aunque normalmente _fds está vacío aquí)
		size_t jj = 0;
		while (jj < _fds.size())
		{
			close(_fds[jj].fd);
			++jj;
		}
		_fds.clear();
		_clients.clear();

		return (false);
	}
	// Hacer socket no bloqueante
	if (Server::setNonBlocking(_server_fd) == -1)
	{
		std::cerr << "ERROR: no se pudo poner server_fd non-blocking\n";
		// limpiar y salir: cerrar server_fd y cualquier otro fd abierto
		if (_server_fd != -1)
		{
			close(_server_fd);
			_server_fd = -1;
		}
		size_t jj = 0;
		while (jj < _fds.size())
		{
			close(_fds[jj].fd);
			++jj;
		}
		_fds.clear();
		_clients.clear();

		return (false);
	}
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

// Lee los bytes que el cliente envía por su socket, ejecuta sus comandos, registra usuarios y gestiona desconexiones
bool Server::handleClientEvent(size_t i)
{
	// Lectura normal en cliente (POLLIN)
	if (!(_fds[i].revents & POLLIN))
		return (false);

	int clientFd = _fds[i].fd;
	ssize_t n = recv(clientFd, _buf, BUF_SIZE, 0); //la cantidad de bytes que ha enviado el cliente al servidor

	if (n > 0)
	{
		_clients[clientFd].accumulator.append(_buf, _buf + n);
		size_t pos;
		while ((pos = _clients[clientFd].accumulator.find('\n')) != std::string::npos)
		{
			std::string line = _clients[clientFd].accumulator.substr(0, pos);
			if (!line.empty() && line[line.size() - 1] == '\r')
				line.erase(line.size() - 1);

			std::cout << "fd " << clientFd << " -> Línea recibida: [" << line << "]\n";
			if (!_clients[clientFd].registered)
			{
				// Siempre permitir PASS y CAP antes del registro
				handleAuthenticationCmds(i, line);

				// Cliente NO registrado aún
				if (line.compare(0, 5, "NICK ") == 0)
					handleNickCommand(clientFd, line);
				else if (line.compare(0, 5, "USER ") == 0)
					handleUserCommand(clientFd, line);
				else if (line.compare(0, 4, "JOIN") == 0 || line.compare(0, 7, "PRIVMSG") == 0 || line.compare(0, 4, "KICK") == 0 || line.compare(0, 6, "INVITE") == 0
						|| line.compare(0, 5, "TOPIC") == 0 || line.compare(0, 4, "MODE") == 0 || line.compare(0, 4, "PART") == 0)
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
					sendNumericMessage(clientFd, "451 :You have not registered"); // ERR_NOTREGISTERED 451
				}

				// Si ya tenemos PASS correcto + NICK + USER → registrar
				if (_clients[clientFd].correctPass && !_clients[clientFd].nickname.empty() && !_clients[clientFd].username.empty())
				{
					_clients[clientFd].registered = true;
					std::string welcome = "001 " + _clients[clientFd].nickname + " :Welcome to the simple IRCd";
					sendNumericMessage(clientFd, welcome);
					std::cout << "fd " << clientFd << " registrado (PASS+NICK+USER). Enviada 001.\n"; // enviar RPL_WELCOME (001)
				}
			}
			else
			{
				// como irssi manda CAP para imprimir mensaje en el servidor de que lo ignoramos
				if (line.compare(0, 4, "CAP ") == 0)
					handleAuthenticationCmds(i, line);
				//JOIN = para conectarte a un canal, los canales se llaman con prefijos: # ! & +
				else if (line.compare(0, 5, "JOIN ") == 0)
					handleJoinCommand(clientFd, line);
				//PART = salir de un canal
				else if (line.compare(0, 5, "PART ") == 0)
					handlePartCommand(clientFd, line);
				//PRIVMSG = mensaje privado
				else if (line.compare(0, 8, "PRIVMSG ") == 0)
					handlePrivmsgCommand(clientFd, line);
				//KICK = operator expulsa a un regular user de un caanal
				else if (line.compare(0, 5, "KICK ") == 0)
					handleKickCommand(clientFd, line);
				//INVITE = operator invita a un cliente a un canaal
				else if (line.compare(0, 7, "INVITE ") == 0)
					handleInviteCommand(clientFd, line);
				//TOPIC = Ver el topic del canal o cambiarlo
				else if (line.compare(0, 6, "TOPIC ") == 0)
					handleTopicCommand(clientFd, line);
				//MODE = operator puede cambiar diversas cosas con la flag +i +t +k +o +l
				else if (line.compare(0, 5, "MODE ") == 0)
					handleChannelModes(clientFd, line);
				// IRSSI(cliente) esta mandando PING y si no contestamos PONG IRSSI(cliente) se desconecta
				else if (line.compare(0, 5, "PING ") == 0)
				{
					std::string ping_target = line.substr(5);
					sendNumericMessage(clientFd, "PONG " + ping_target);
					std::cout << "fd " << clientFd << " -> Respondido PONG a [" << ping_target << "]\n";
				}
				//una vez ya autorizado respondemos si nos ponen denuevo estos comandos de autorizacion
				else if (line.compare(0, 5, "PASS ") == 0 || line.compare(0, 5, "USER ") == 0)
				{
					std::string cur = _clients[clientFd].nickname;
					if (cur.empty())
						cur = "*";
					sendNumericMessage(clientFd, ":ircserv 462 " + cur + " :You may not reregister"); // ERR_ALREADYREGISTERED 462
				}
				//NICK = cambiar el nickname una vez el cliente ya esta registrado
				else if (line.compare(0, 5, "NICK ") == 0)
				{
					// Guardamos el nick actual poder saber si cambia
					std::string originalNick = _clients[clientFd].nickname;

					// Construimos el prefijo antiguo nick!user@host
					std::string displayOldNick;
					if (originalNick.empty())
						displayOldNick = convertIntToString(clientFd);
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
						std::map<int, Client>::iterator it = _clients.begin();
						while (it != _clients.end())
						{
							sendNumericMessage(it->first, out);
							++it;
						}
					}
				}
				else
					sendNumericMessage(clientFd, "421 :Unknown command"); //Comandos no implementados o desconocidgos
			}
			_clients[clientFd].accumulator.erase(0, pos + 1);
		}
	}
	else if (n == 0) // Cliente cerró conexión de forma ordenada -> limpiar también transfers relacionados
	{
		int closedFd = clientFd;
		std::cout << "Cliente (fd " << closedFd << ") cerró conexión\n";

		cleanupTransfersForClient(closedFd); // Limpiar transfers que referencien este cliente como sender/receiver

		close(closedFd); // ahora cerrar y borrar cliente
		_clients.erase(closedFd);
		_fdToTransferId.erase(closedFd);
		_fds.erase(_fds.begin() + i);
		return (true);
	}
	else
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		{
			std::cerr << "ERROR: recv falló\n";
			int bad = clientFd;

			cleanupTransfersForClient(bad); // limpiar transfers asociados

			close(clientFd); // cerrar el cliente problemático
			_clients.erase(clientFd);
			_fdToTransferId.erase(clientFd);
			_fds.erase(_fds.begin() + i);
			return (true);
		}
	}
	return (false);
}

// Bucle principal que gestiona conexiones, mensajes y eventos del servidor
int Server::runServerLoop()
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
			if (handleFileTransferEvent(i))
				continue;

			//Error o desconexión en un cliente o en el servidor 
			// POLLERR=conexion rota, POLLHUP=cierras la terminal, POLLNVAL=fd corrupto
			if (_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				// Si el fd desconectado tiene asociado algun transfer (mapping), y además es cliente "normal", debemos limpiar las transferencias relacionadas (sender o receiver)
				int badfd = _fds[i].fd;

				// Si el fd es server_fd -> error crítico
				if (badfd == _server_fd)
				{
					std::cerr << "ERROR en server_fd (poll)\n";
					g_running = 0;
					break;
				}
				cleanupTransfersForClient(badfd); // Antes de cerrar, limpiar transfers que referencien este cliente como sender/receiver

				// Ahora cerramos y borramos el cliente (como antes)
				std::cout << "Cliente (fd " << badfd << ") se desconectó/err\n";
				close(badfd);
				_clients.erase(badfd);
				_fdToTransferId.erase(badfd);
				_fds.erase(_fds.begin() + i);
				continue;
			}
			if (_fds[i].fd == _server_fd && (_fds[i].revents & POLLIN)) // Nueva conexión entrante, POLLIN=poll recibe datos a leer
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
					if (Server::setNonBlocking(client_fd) == -1)
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
			if (handleClientEvent(i)) // Lectura normal en cliente (POLLIN)
				continue;
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

				std::string &data = _clients[fd].outBuffer;

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
						
						cleanupTransfersForClient(badfd); // limpiar transfers relacionados (igual que antes)

						// quitar entrada de _fds correspondiente al cliente (ya cerrada arriba)
						_fds.erase(_fds.begin() + i);
						continue;
					}
				}

				//Si ya terminé de enviar todo al cliente, deja de preguntarle al sistema si puedo escribir
				if (_clients.find(fd) != _clients.end() && _clients[fd].outBuffer.empty())
					_fds[i].events &= ~POLLOUT; /*esta linea=Deja de vigilar escritura para este socket y la ~ es para invertir todos los bits de POLLOUT*/
			}
			++i; // avanzar al siguiente fd
		}
	}
	//Mensaje de que el servidor se ha cerrado con Ctr+c o Ctr+\ o kill
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
