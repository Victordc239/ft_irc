/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 15:51:36 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/03 12:37:55 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstdlib>
#include <csignal>

volatile sig_atomic_t g_running = 1;

void sigint_handler(int signal)
{
	(void)signal; // ignorar parámetro
	g_running = 0; // avisar al main loop que debe salir
}

/*esta funcion hace que el socket no sea blocante, es decir que el primer cliente
bloquearia a los siguientes si no envia nada y los siguientes se quieren conectar o mandar algo*/
int set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cerr << "ERROR: error arguments\n";
		return 1;
	}

	//manejar señales de interrupcion
	signal(SIGINT, sigint_handler); //Ctrl+C
	signal(SIGQUIT, sigint_handler); /*Ctrl+\ */ 
	signal(SIGPIPE, SIG_IGN); // Escribir a socket cerrado

	// parsear puerto
	char *endptr = NULL;
	long port = strtol(argv[1], &endptr, 10);
	if (*endptr != '\0' || port <= 0 || port > 65535)
	{
		std::cerr << "Puerto inválido: " << argv[1] << "\n";
		return 1;
	}

	// guardar contraseña
	std::string server_password = argv[2];

	// Crear socket servidor que es el que escucha
	int server_fd = socket(AF_INET, SOCK_STREAM, 0); //AF_INET=IPV4, SOCK_STREAM=TCP, 0=protocolo por defecto
	if (server_fd == -1)
	{
		std::cerr << "ERROR: socket failure\n";
		return 1;
	}

	std::cout << "Socket creado correctamente\n";

	/*es para reusar el mismo puerto porque el kernel cuando cierras pone el puerto en time_wait y sin ello
	no podrias usar el mismo puerto si cerramos el irc y lo volvemos a ejecutar acto seguido*/
	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		/*server_fd=socket, SOL_SOCKET=aplica a todo el socket(no solo al puerto o ip o protocolo),
		SO_REUSEADDR=permite reusar ip y puerto, &opt=activarlo, sizeof(opt)=tamaño valor*/
		std::cerr << "WARNING: setsockopt falló\n";
	}

	// Hacer socket no bloqueante
	if (set_nonblock(server_fd) == -1)
		std::cerr << "WARNING: no se pudo poner server_fd non-blocking\n";

	// Dirección servidor
	sockaddr_in server_addr; //estructura que ya existe para guardar ip y puerto del servidor
	std::memset(&server_addr, 0, sizeof(server_addr)); //seteamos a 0 toda la estructura
	server_addr.sin_family = AF_INET; //IPv4
	server_addr.sin_port = htons((uint16_t)port); //Conversion del numero del puerto en bits para que todos los ordenadores lo interpreten igual
	server_addr.sin_addr.s_addr = INADDR_ANY; //cualquier adapator de red del ordenador, ethernet, wifi, localhost,...

	if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) //le decimos a que puerto y ip esta asociado el socket
	{
		std::cerr << "ERROR: bind failure\n";
		close(server_fd);
		return 1;
	}

	std::cout << "Bind hecho correctamente\n";

	if (listen(server_fd, SOMAXCONN) == -1)
	{
		/*convierte el socket en un servidor que escucha todo lo que le manden los clientes*/
		/*SOMAXCONN = valor del sistema que representa la máxima cantidad de conexiones pendientes permitidas*/
		std::cerr << "ERROR: listen failure\n";
		close(server_fd);
		return 1;
	}

	std::cout << "Servidor escuchando...\n";

	// Estructura poll
	std::vector<struct pollfd> fds;
	pollfd pfd;
	pfd.fd = server_fd; //vigilar el servidor
	pfd.events = POLLIN; //avisame cuando haya datos a leer
	pfd.revents = 0; //eventos que han pasado
	fds.push_back(pfd);

	// Acumuladores por cliente para recibir todos los mensajes completos
	std::map<int, std::string> accum;

	//Buffer para enviar mensaje de servidor a cliente
	std::map<int, std::string> outbuf;

	//mapa para verificar si cada fd client a puesto correctamente la contraseña
	std::map<int, bool> correctPass;

	const size_t BUF_SIZE = 512;
	char buf[BUF_SIZE]; //buffer para almacenar el mensaje que envias

	while (g_running)
	{
		int ret = poll(&fds[0], fds.size(), -1); // espera que haya algun evento en un cliente o en el servidor
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "ERROR: poll falló\n";
			break;
		}

		// Recorremos fds; cuidado al eliminar: recorremos con índice
		for (size_t i = 0; i < fds.size(); ++i)
		{
			if (fds[i].revents == 0)
				continue;

			/*Error o desconexión en un cliente o en el servidor 
			POLLERR=conexion rota, POLLHUP=cierras la terminal, POLLNVAL=fd corrupto*/
			if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (fds[i].fd == server_fd)
				{
					std::cerr << "ERROR en server_fd (poll)\n";
					g_running = 0;
					break;
				}
				else
				{
					std::cout << "Cliente (fd " << fds[i].fd << ") se desconectó/err\n";
					close(fds[i].fd);
					accum.erase(fds[i].fd);
					outbuf.erase(fds[i].fd);
					correctPass.erase(fds[i].fd);
					fds.erase(fds.begin() + i);
					--i;
					continue;
				}
			}

			// Nueva conexión entrante, POLLIN=poll recibe datos a leer
			if (fds[i].fd == server_fd && (fds[i].revents & POLLIN))
			{
				// aceptar tantas conexiones como estén pendientes
				while (true)
				{
					sockaddr_in client_addr;
					socklen_t client_len = sizeof(client_addr);
					int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len); /*acepta y crea
							un nuevo socket del nuevo cliente y devuelve el fd del nuevo cliente
							(sockaddr*)&client_addr=la ip y el puerto del nuevo cliente*/
					if (client_fd == -1)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;  // no hay más conexiones pendientes
						std::cerr << "ERROR en accept nuevo cliente\n";
						break;
					}
					
					// opcional: poner non-blocking al cliente
					if (set_nonblock(client_fd) == -1)
						std::cerr << "WARNING: no se pudo poner client_fd non-blocking\n";

					// agregar a poll
					pollfd cp;
					cp.fd = client_fd;
					cp.events = POLLIN;
					cp.revents = 0;
					fds.push_back(cp);

					// inicializar acumulador para este nuevo cliente
					accum[client_fd] = "";

					// inicializar buffer salida del servidor para este nuevo cliente
					outbuf[client_fd] = "";

					// inicializar estado de autenticación (necesita PASS)
					correctPass[client_fd] = false;

					// imprimir IP del cliente
					char client_ip[INET_ADDRSTRLEN];
					if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL)
						std::cout << "Cliente conectado (fd " << client_fd << ") desde [ip desconocida]:" << ntohs(client_addr.sin_port) << "!\n";
					else
						std::cout << "Cliente conectado desde " << client_ip << ":" << ntohs(client_addr.sin_port) << " (fd " << client_fd << ")\n";
				}
				// fin manejo server_fd
				continue;
			}

			// Datos desde un cliente existente
			if (fds[i].revents & POLLIN)
			{
				int clientFd = fds[i].fd;

				while (true)
				{
					ssize_t n = recv(clientFd, buf, BUF_SIZE, 0); //la cantidad de bytes que ha enviado el cliente al servidor

					if (n > 0)
					{
						// añadir a acumulador
						accum[clientFd].append(buf, buf + n);

						// procesar líneas completas
						size_t pos;
						while ((pos = accum[clientFd].find('\n')) != std::string::npos)
						{
							std::string line = accum[clientFd].substr(0, pos);
							if (!line.empty() && line[line.size() - 1] == '\r')
								line.erase(line.size() - 1);

							std::cout << "fd " << clientFd << " -> Línea recibida: [" << line << "]\n";

							if (!correctPass[clientFd])
							{
								// esperamos "PASS <password>"
								if (line.compare(0, 5, "PASS ") == 0)
								{
									std::string given = line.substr(5);
									if (given == server_password)
									{
										correctPass[clientFd] = true;
										outbuf[clientFd] += "PASS accepted\r\n";
										fds[i].events |= POLLOUT;
										std::cout << "fd " << clientFd << " autenticado correctamente\n";
									}
									else
									{
										std::cout << "fd " << clientFd << " fallo autenticacion. Cerrando.\n";
										close(clientFd);
										accum.erase(clientFd);
										outbuf.erase(clientFd);
										correctPass.erase(clientFd);
										fds.erase(fds.begin() + i);
										--i;
										break;
									}
								}
								else
								{
									std::cout << "fd " << clientFd << " no envió PASS primero. Cerrando.\n";
									close(clientFd);
									accum.erase(clientFd);
									outbuf.erase(clientFd);
									correctPass.erase(clientFd);
									fds.erase(fds.begin() + i);
									--i;
									break;
								}
							}
							else
							{
								outbuf[clientFd] += "Servidor dice: hola\r\n";
								fds[i].events |= POLLOUT;
							}

							accum[clientFd].erase(0, pos + 1);
						}
					}
					else if (n == 0) 	// cliente cerró conexión
					{
						std::cout << "Cliente (fd " << clientFd << ") cerró conexión\n";
						close(clientFd);
						accum.erase(clientFd);
						outbuf.erase(clientFd);
						correctPass.erase(clientFd);
						fds.erase(fds.begin() + i);
						--i;
						break;
					}
					else //ERROR al recibir los datos
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;
						std::cerr << "ERROR: recv falló\n";
						// cerramos y limpiamos
						close(clientFd);
						accum.erase(clientFd);
						outbuf.erase(clientFd);
						correctPass.erase(clientFd);
						fds.erase(fds.begin() + i);
						--i;
						break;
					}
				}
			}

			// Enviar datos a cliente si está listo para escribir
			if (fds[i].revents & POLLOUT)
			{
				int fd = fds[i].fd;
				std::string &data = outbuf[fd];

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
						accum.erase(fd);
						outbuf.erase(fd);
						correctPass.erase(fd);
						fds.erase(fds.begin() + i);
						--i;
						break;
					}
				}

				if (outbuf.find(fd) != outbuf.end() && outbuf[fd].empty()) /*Si ya terminé de enviar todo al cliente, deja de preguntarle al sistema si puedo escribir*/
					fds[i].events &= ~POLLOUT; /*esta linea=Deja de vigilar escritura para este socket y la ~ es para invertir todos los bits de POLLOUT*/
			}
		} // fin for fds
	} // fin while poll

	// limpieza (no debería llegar aquí normalmente)
	for (size_t j = 0; j < fds.size(); ++j)
		close(fds[j].fd);

	return 0;
}