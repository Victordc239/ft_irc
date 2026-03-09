/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 15:51:36 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/09 17:14:53 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <cstdlib>
#include "Server.hpp"

volatile sig_atomic_t g_running = 1;

void sigint_handler(int signal)
{
	(void)signal; // ignorar parámetro
	g_running = 0; // avisar al main loop que debe salir
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
	signal(SIGTERM, sigint_handler); // kill, systemd stops, etc
	signal(SIGPIPE, SIG_IGN); // Escribir a socket cerrado

	// parsear puerto
	char *endptr = NULL;
	long port = strtol(argv[1], &endptr, 10);
	if (*endptr != '\0' || port <= 0 || port > 65535)
	{
		std::cerr << "Puerto inválido: " << argv[1] << "\n";
		return 1;
	}

	Server server;
	if (!server.init_and_listen(port, argv[2]))
		return 1;

	return server.run_loop();
}
