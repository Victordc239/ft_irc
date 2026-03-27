/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 15:51:36 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/27 15:50:29 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <cstdlib>
#include "Server.hpp"

volatile sig_atomic_t g_running = 1;

void sigintHandler(int signal)
{
	(void)signal;
	g_running = 0;
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cerr << "ERROR: error arguments\n";
		return (1);
	}

	signal(SIGINT, sigintHandler); // "Ctrl+C"
	signal(SIGQUIT, sigintHandler); // "Ctrl+\"   
	signal(SIGTSTP, sigintHandler); // "Ctrl+Z"
	signal(SIGTERM, sigintHandler); // kill, systemd stops, etc
	signal(SIGPIPE, SIG_IGN); // Write to closed socket

	char *endptr = NULL;
	long port = ft_strtol(argv[1], &endptr);
	if (*endptr != '\0' || port <= 0 || port > 65535)
	{
		std::cerr << "Error. Invalid port: " << argv[1] << "\n";
		return (1);
	}

	Server server;
	if (!server.InitSocketAndListen(port, argv[2]))
		return (1);

	return (server.runServerLoop());
}
