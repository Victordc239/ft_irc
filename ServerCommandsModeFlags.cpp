/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCommandsModeFlags.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/10 14:47:22 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/16 22:44:09 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

void Server::mode_i(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &prefix)
{
	// solo operadores pueden cambiar opciones del canal
	if (!channel.isOperator(clientFd))
	{
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	channel.setInviteOnly(plus);

	std::string out = ":" + prefix + " MODE " + channelName + " ";
	if (plus)
		out += "+i";
	else
		out += "-i";

	std::set<int>::iterator iterator = channel.clients.begin();
	while (iterator != channel.clients.end())
	{
		int fd = *iterator;
		if (_clients.find(fd) != _clients.end())
			sendNumeric(fd, out);
		++iterator;
	}
}

void Server::mode_t(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &prefix)
{
	if (!channel.isOperator(clientFd))
	{
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	channel.setTopicRestricted(plus);

	std::string out = ":" + prefix + " MODE " + channelName + " ";
	if (plus)
		out += "+t";
	else
		out += "-t";

	std::set<int>::iterator iterator = channel.clients.begin();
	while (iterator != channel.clients.end())
	{
		int fd = *iterator;
		if (_clients.find(fd) != _clients.end())
			sendNumeric(fd, out);
		++iterator;
	}
}

void Server::mode_k(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &param, const std::string &prefix)
{
	if (!channel.isOperator(clientFd))
	{
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	if (plus)
	{
		// establecer key (param contiene la key)
		channel.setKey(param);
		std::string out = ":" + prefix + " MODE " + channelName + " +k " + param;

		std::set<int>::iterator iterator = channel.clients.begin();
		while (iterator != channel.clients.end())
		{
			int fd = *iterator;
			if (_clients.find(fd) != _clients.end())
				sendNumeric(fd, out);
			++iterator;
		}
	}
	else
	{
		// quitar key
		channel.removeKey();
		std::string out = ":" + prefix + " MODE " + channelName + " -k";

		std::set<int>::iterator iterator = channel.clients.begin();
		while (iterator != channel.clients.end())
		{
			int fd = *iterator;
			if (_clients.find(fd) != _clients.end())
				sendNumeric(fd, out);
			++iterator;
		}
	}
}

void Server::mode_o(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &targetNick, const std::string &prefix)
{
	// sólo operadores pueden dar/quitar operador
	if (!channel.isOperator(clientFd))
	{
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	int targetFd = getFdByNick(targetNick);
	if (targetFd == -1)
	{
		sendNumeric(clientFd, "401 " + targetNick + " :No such nick");
		return;
	}

	if (!channel.hasClient(targetFd))
	{
		sendNumeric(clientFd, "441 " + targetNick + " " + channelName + " :They aren't on that channel");
		return;
	}

	if (plus)
		channel.addOperator(targetFd);
	else
		channel.removeOperator(targetFd);

	std::string sign;
	if (plus)
		sign = "+o";
	else
		sign = "-o";

	std::string out = ":" + prefix + " MODE " + channelName + " " + sign + " " + targetNick;

	std::set<int>::iterator iterator = channel.clients.begin();
	while (iterator != channel.clients.end())
	{
		int fd = *iterator;
		if (_clients.find(fd) != _clients.end())
			sendNumeric(fd, out);
		++iterator;
	}
}

void Server::mode_l(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &param, const std::string &prefix)
{
	if (!channel.isOperator(clientFd))
	{
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	if (plus)
	{
		// param debería ser número
		const char *s = param.c_str();
		char *endptr = NULL;
		long v = ftStrtol(s, &endptr);
		if (*endptr != '\0' || v < 0)
		{
			sendNumeric(clientFd, "461 MODE :Not enough parameters");
			return;
		}
		channel.setLimit((int)v);
		std::string out = ":" + prefix + " MODE " + channelName + " +l " + intToString((int)v);

		std::set<int>::iterator iterator = channel.clients.begin();
		while (iterator != channel.clients.end())
		{
			int fd = *iterator;
			if (_clients.find(fd) != _clients.end())
				sendNumeric(fd, out);
			++iterator;
		}
	}
	else
	{
		channel.removeLimit();
		std::string out = ":" + prefix + " MODE " + channelName + " -l";

		std::set<int>::iterator iterator = channel.clients.begin();
		while (iterator != channel.clients.end())
		{
			int fd = *iterator;
			if (_clients.find(fd) != _clients.end())
				sendNumeric(fd, out);
			++iterator;
		}
	}
}

void Server::mode_user(int clientFd, const std::string &target, const std::string &rest)
{
	// target debe ser el nick; solo el propio usuario puede cambiar sus UMODE
	std::map<int, Client>::iterator itClient = _clients.find(clientFd);
	if (itClient == _clients.end())
		return;

	std::string myNick = itClient->second.nickname;
	if (myNick.empty() || myNick != target)
	{
		// ERR_USERSDONTMATCH 502
		sendNumeric(clientFd, "502 :Cannot change mode for other users");
		return;
	}

	// extraer el token de modos (primer token de rest)
	std::string modes = rest;
	// trim inicio
	while (!modes.empty() && (modes[0] == ' ' || modes[0] == '\t'))
		modes.erase(0, 1);
	// si hay espacios, tomar sólo hasta el primero
	size_t space = modes.find(' ');
	if (space != std::string::npos)
		modes = modes.substr(0, space);
	// quitar CR final si existe
	if (!modes.empty() && modes[modes.size() - 1] == '\r')
		modes.erase(modes.size() - 1);

	if (modes.empty())
	{
		// petición de ver modos del usuario: podemos devolver un echo simple
		std::string reply = ":ircserv 221 " + myNick + " :User modes";
		sendNumeric(clientFd, reply);
		return;
	}

	bool plus = true;
	std::string applied; // para construir el eco final (ej "+i" o "-i")
	for (size_t i = 0; i < modes.size(); ++i)
	{
		char c = modes[i];
		if (c == '+')
		{
			plus = true;
			continue;
		}
		if (c == '-')
		{
			plus = false;
			continue;
		}

		if (c == 'i')
		{
			_clients[clientFd].invisible = plus;

			if (plus)
				std::cout << "fd " << clientFd << "Usuario " << myNick << " ahora es invisible\n";
			else
				std::cout << "fd " << clientFd << "Usuario " << myNick << " ya no es invisible\n";

			// añadir a applied; simplificamos: concatenamos +i/-i según corresponda
			if (plus)
				applied += "+i";
			else
				applied += "-i";
		}
		else
			sendNumeric(clientFd, "501 :Unknown MODE flag"); // UMODEUNKNOWNFLAG 501
	}

	if (!applied.empty())
	{
		// Enviar eco al propio cliente (y podrías notificar a otros según necesidades)
		std::string echo = ":" + myNick + " MODE " + target + " " + applied;
		sendNumeric(clientFd, echo);
	}
}
