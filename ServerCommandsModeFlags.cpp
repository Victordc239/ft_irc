/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCommandsModeFlags.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/10 14:47:22 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/10 17:18:31 by vdiez-cu         ###   ########.fr       */
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
		if (this->_clients.find(fd) != this->_clients.end())
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
		if (this->_clients.find(fd) != this->_clients.end())
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
			if (this->_clients.find(fd) != this->_clients.end())
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
			if (this->_clients.find(fd) != this->_clients.end())
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
		if (this->_clients.find(fd) != this->_clients.end())
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
			if (this->_clients.find(fd) != this->_clients.end())
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
			if (this->_clients.find(fd) != this->_clients.end())
				sendNumeric(fd, out);
			++iterator;
		}
	}
}