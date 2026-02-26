/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 15:51:36 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/02/26 16:09:42 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		std::cerr << "ERROR: socket failure\n";
		return 1;
	}

	std::cout << "Socket creado correctamente\n";

	// 1️⃣ Creamos la dirección del servidor
	sockaddr_in server_addr;

	server_addr.sin_family = AF_INET;       // IPv4
	server_addr.sin_port = htons(1201);     // Puerto
	server_addr.sin_addr.s_addr = INADDR_ANY; // Acepta conexiones desde cualquier IP

	// 2️⃣ Conectamos el socket al puerto
	if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		std::cerr << "ERROR: bind failure\n";
		return 1;
	}

	std::cout << "Bind hecho correctamente\n";

	if (listen(server_fd, SOMAXCONN) == -1)
	{
		std::cerr << "ERROR: listen failure\n";
		return 1;
	}

	std::cout << "Servidor escuchando...\n";

	sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	std::cout << "Esperando cliente...\n";

	int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
	if (client_fd == -1)
	{
		std::cerr << "ERROR: accept failure\n";
		return 1;
	}

	std::cout << "Cliente conectado!\n";
	
}