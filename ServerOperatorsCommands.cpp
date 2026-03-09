/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerOperatorsCommands.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/09 14:39:18 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/09 15:20:11 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

void Server::handle_kick_command(int clientFd, const std::string &line)
{
	// "KICK " tiene longitud 5
	const size_t prefix_len = 5;
	if (line.size() <= prefix_len)
	{
		send_numeric(clientFd, "461 KICK :Not enough parameters");
		return;
	}

	// extraer canal (primer token tras "KICK ")
	size_t sp = line.find(' ', prefix_len);
	if (sp == std::string::npos)
	{
		send_numeric(clientFd, "461 KICK :Not enough parameters");
		return;
	}
	std::string chan = line.substr(prefix_len, sp - prefix_len);

	// trim simple de chan (inicio/fin)
	while (!chan.empty() && (chan[0] == ' ' || chan[0] == '\t'))
		chan.erase(0, 1);
	while (!chan.empty() && (chan[chan.size() - 1] == ' ' || chan[chan.size() - 1] == '\t'))
		chan.erase(chan.size() - 1, 1);

	// extraer nick objetivo y comentario opcional
	std::string targetNick;
	std::string comment;

	// buscar ':' que indica inicio de comment
	size_t colon = line.find(':', sp + 1);

	if (colon == std::string::npos)
	{
		// no hay comment, el nick va hasta el final
		targetNick = line.substr(sp + 1);
		// trim final CR si hay
		if (!targetNick.empty() && targetNick[targetNick.size() - 1] == '\r')
			targetNick.erase(targetNick.size() - 1, 1);
	}
	else
	{
		// nick está entre sp+1 y colon-1
		if (colon > sp + 1)
			targetNick = line.substr(sp + 1, colon - (sp + 1));
		else
			targetNick = "";
		// comentario después de ':'
		comment = line.substr(colon + 1);
		// trim CR final en comment
		if (!comment.empty() && comment[comment.size() - 1] == '\r')
			comment.erase(comment.size() - 1, 1);
	}

	// trim sencillo en targetNick
	while (!targetNick.empty() && (targetNick[0] == ' ' || targetNick[0] == '\t'))
		targetNick.erase(0, 1);
	while (!targetNick.empty() && (targetNick[targetNick.size() - 1] == ' ' || targetNick[targetNick.size() - 1] == '\t'))
		targetNick.erase(targetNick.size() - 1, 1);

	if (targetNick.empty())
	{
		send_numeric(clientFd, "461 KICK :Not enough parameters");
		return;
	}

	// ver si existe el canal
	std::map<std::string, Channel>::iterator it = this->_channels.find(chan);
	if (it == this->_channels.end())
	{
		send_numeric(clientFd, "403 " + chan + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// el emisor está en el canal?
	if (!channel.hasClient(clientFd))
	{
		send_numeric(clientFd, "442 " + chan + " :You're not on that channel");
		return;
	}

	// el emisor es operador?
	if (!channel.isOperator(clientFd))
	{
		send_numeric(clientFd, "482 " + chan + " :You're not channel operator");
		return;
	}

	// existe el nick objetivo en el servidor?
	int targetFd = get_fd_by_nick(targetNick);
	if (targetFd == -1)
	{
		send_numeric(clientFd, "401 " + targetNick + " :No such nick");
		return;
	}

	// el objetivo está en el canal?
	if (!channel.hasClient(targetFd))
	{
		send_numeric(clientFd, "441 " + targetNick + " " + chan + " :They aren't on that channel");
		return;
	}

	// Construir prefijo: nick!user@localhost (igual que en otros comandos)
	std::string emNick = this->_clients[clientFd].nickname;
	std::string emUser = this->_clients[clientFd].username;
	if (emNick.empty())
		emNick = intToString(clientFd);
	if (emUser.empty())
		emUser = "user";
	std::string prefix = emNick + "!" + emUser + "@localhost";

	// Construir mensaje KICK
	std::string out = ":" + prefix + " KICK " + chan + " " + targetNick;
	if (!comment.empty())
		out += " :" + comment;
	else
		out += " :";

	// Enviar el KICK a todos los miembros (siempre comprobando que existan en _clients)
	std::set<int>::iterator sit = channel.clients.begin();
	while (sit != channel.clients.end())
	{
		int fd = *sit;
		if (this->_clients.find(fd) != this->_clients.end())
			send_numeric(fd, out);
		++sit;
	}

	// Quitar al cliente del canal (y si era operador, Channel::removeClient ya lo quita de operators)
	channel.removeClient(targetFd);

	// Si el canal queda vacío, eliminarlo del mapa
	if (channel.clients.empty())
		this->_channels.erase(it);
}