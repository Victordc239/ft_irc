/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 15:51:36 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/02 11:19:20 by victor           ###   ########.fr       */
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
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>

/*esta funcion hace que el socket no sea blocante, es decir que el primer cliente
bloquearia a los siguientes si no envia nada y los siguientes se quieren conectar o mandar algo*/
int set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main()
{
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
	server_addr.sin_port = htons(1201); //Conversion del numero del puerto en bits para que todos los ordenadores lo interpreten igual
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
	pfd.fd = server_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	fds.push_back(pfd);

	// Acumuladores por cliente
	std::map<int, std::string> accum;

	const size_t BUF_SIZE = 512;
	char buf[BUF_SIZE];

	while (true)
	{
		int ret = poll(&fds[0], fds.size(), -1); // espera indefinida
		if (ret == -1)
		{
			std::cerr << "ERROR: poll falló\n";
			break;
		}

		// Recorremos fds; cuidado al eliminar: recorremos con índice
		for (size_t i = 0; i < fds.size(); ++i)
		{
			if (fds[i].revents == 0)
				continue;

			// Error o desconexión
			if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (fds[i].fd == server_fd)
				{
					std::cerr << "ERROR en server_fd (poll)\n";
					// en caso crítico cerramos todo
					close(server_fd);
					return 1;
				}
				else
				{
					int badfd = fds[i].fd;
					std::cout << "Cliente (fd " << badfd << ") se desconectó/err\n";
					close(badfd);
					accum.erase(badfd);
					fds.erase(fds.begin() + i);
					--i;
					continue;
				}
			}

			// Nueva conexión entrante
			if (fds[i].fd == server_fd && (fds[i].revents & POLLIN))
			{
				// aceptar tantas conexiones como estén pendientes
				while (true)
				{
					sockaddr_in client_addr;
					socklen_t client_len = sizeof(client_addr);
					int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
					if (client_fd == -1)
					{
						// si errno == EWOULDBLOCK/EAGAIN no hay más conexiones pendientes
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

					// inicializar acumulador
					accum[client_fd] = "";

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
				int cfd = fds[i].fd;
				ssize_t n = recv(cfd, buf, BUF_SIZE, 0);
				if (n == -1)
				{
					// EWOULDBLOCK/EAGAIN puede suceder si non-blocking; ignoramos y seguimos
					std::cerr << "ERROR: recv falló en fd " << cfd << "\n";
					// cerramos y limpiamos
					close(cfd);
					accum.erase(cfd);
					fds.erase(fds.begin() + i);
					--i;
					continue;
				}
				if (n == 0)
				{
					// cliente cerró conexión
					std::cout << "Cliente (fd " << cfd << ") cerró conexión\n";
					close(cfd);
					accum.erase(cfd);
					fds.erase(fds.begin() + i);
					--i;
					continue;
				}

				// añadir a acumulador
				accum[cfd].append(buf, buf + n);

				// procesar líneas completas
				size_t pos;
				while ((pos = accum[cfd].find('\n')) != std::string::npos)
				{
					std::string line = accum[cfd].substr(0, pos);
					if (!line.empty() && line[line.size() - 1] == '\r')
						line.erase(line.size() - 1);

					// log en servidor
					std::cout << "fd " << cfd << " -> Línea recibida: [" << line << "]\n";

					// responder con CRLF
					const char reply[] = "Servidor dice: hola\r\n";
					ssize_t sent = send(cfd, reply, std::strlen(reply), 0);
					if (sent == -1)
						std::cerr << "ERROR: send falló en fd " << cfd << "\n";
					else
						std::cout << "fd " << cfd << " <- Respuesta enviada (" << sent << " bytes)\n";

					// borrar línea procesada (incluye '\n')
					accum[cfd].erase(0, pos + 1);
				}
			}
		} // fin for fds
	} // fin while poll

	// limpieza (no debería llegar aquí normalmente)
	for (size_t j = 0; j < fds.size(); ++j)
		close(fds[j].fd);
	return 0;
}