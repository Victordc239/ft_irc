/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerOperatorsCommands.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/09 14:39:18 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/09 17:29:42 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

void	Server::handle_kick_command(int clientFd, const std::string &line)
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
	std::string channelName = line.substr(prefix_len, sp - prefix_len);

	// trim simple de channelName (inicio/fin)
	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

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
	std::map<std::string, Channel>::iterator it = this->_channels.find(channelName);
	if (it == this->_channels.end())
	{
		send_numeric(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// el emisor está en el canal?
	if (!channel.hasClient(clientFd))
	{
		send_numeric(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}

	// el emisor es operador?
	if (!channel.isOperator(clientFd))
	{
		send_numeric(clientFd, "482 " + channelName + " :You're not channel operator");
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
		send_numeric(clientFd, "441 " + targetNick + " " + channelName + " :They aren't on that channel");
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
	std::string out = ":" + prefix + " KICK " + channelName + " " + targetNick;
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

void	Server::handle_invite_command(int clientFd, const std::string &line)
{
	// "INVITE " tiene longitud 7
	const size_t prefix_len = 7;
	if (line.size() <= prefix_len)
	{
		send_numeric(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	// extraer nick objetivo (primer token tras "INVITE ")
	size_t sp = line.find(' ', prefix_len);
	if (sp == std::string::npos)
	{
		send_numeric(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	std::string targetNick = line.substr(prefix_len, sp - prefix_len);

	// el resto es el canal (aceptamos "INVITE nick #channelName" y también "INVITE nick :#channelName")
	std::string channelName;
	size_t channelNameStart = sp + 1;
	if (channelNameStart >= line.size())
	{
		send_numeric(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	// si empieza con ':' (p.e. "INVITE nick :#channelName"), saltar ':'
	if (line[channelNameStart] == ':')
		++channelNameStart;

	channelName = line.substr(channelNameStart);
	// quitar CR final si existe
	if (!channelName.empty() && channelName[channelName.size() - 1] == '\r')
		channelName.erase(channelName.size() - 1, 1);

	// trim sencillo de ambos tokens (inicio/fin espacios)
	while (!targetNick.empty() && (targetNick[0] == ' ' || targetNick[0] == '\t'))
		targetNick.erase(0, 1);
	while (!targetNick.empty() && (targetNick[targetNick.size() - 1] == ' ' || targetNick[targetNick.size() - 1] == '\t'))
		targetNick.erase(targetNick.size() - 1, 1);

	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

	if (targetNick.empty() || channelName.empty())
	{
		send_numeric(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	// comprobar que existe el nick objetivo en el servidor
	int targetFd = get_fd_by_nick(targetNick);
	if (targetFd == -1)
	{
		send_numeric(clientFd, "401 " + targetNick + " :No such nick");
		return;
	}

	// comprobar que existe el canal
	std::map<std::string, Channel>::iterator it = this->_channels.find(channelName);
	if (it == this->_channels.end())
	{
		send_numeric(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// el emisor está en el canal?
	if (!channel.hasClient(clientFd))
	{
		send_numeric(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}

	// el objetivo ya está en el canal?
	if (channel.hasClient(targetFd))
	{
		// ERR_USERONCHANNEL 443
		send_numeric(clientFd, "443 " + targetNick + " " + channelName + " :is already on channel");
		return;
	}

	// Construir prefijo: nick!user@localhost
	std::string emNick = this->_clients[clientFd].nickname;
	std::string emUser = this->_clients[clientFd].username;
	if (emNick.empty())
		emNick = intToString(clientFd);
	if (emUser.empty())
		emUser = "user";
	std::string prefix = emNick + "!" + emUser + "@localhost";

	// Mensaje INVITE que recibe el usuario invitado
	std::string inviteMsg = ":" + prefix + " INVITE " + targetNick + " " + channelName;
	send_numeric(targetFd, inviteMsg);

	// Notificar al emisor con RPL_INVITING (341)
	send_numeric(clientFd, "341 " + emNick + " " + targetNick + " " + channelName);

	std::cout << "DEBUG INVITE: fd " << clientFd << " invitó a " << targetNick << " a " << channelName << "\n";
}

void Server::handle_topic_command(int clientFd, const std::string &line)
{
	// Formatos posibles:
	// "TOPIC <#channel>"              -> ver topic
	// "TOPIC <#channel> :<new topic>" -> cambiar topic (el ':' puede ser obligatorio en clientes)
	const size_t prefix_len = 6; // strlen("TOPIC ")
	if (line.size() <= prefix_len)
	{
		send_numeric(clientFd, "461 TOPIC :Not enough parameters");
		return;
	}

	// extraer canal
	size_t sp = line.find(' ', prefix_len);
	std::string channelName;
	std::string rest;
	if (sp == std::string::npos)
	{
		// sólo "TOPIC #channel" (sin espacio extra)
		channelName = line.substr(prefix_len);
		// trim CR si existe
		if (!channelName.empty() && channelName[channelName.size() - 1] == '\r')
			channelName.erase(channelName.size() - 1, 1);
		rest = "";
	}
	else
	{
		channelName = line.substr(prefix_len, sp - prefix_len);
		rest = line.substr(sp + 1);
		// trim CR final si existe
		if (!rest.empty() && rest[rest.size() - 1] == '\r')
			rest.erase(rest.size() - 1, 1);
	}

	// trim simple channelName (inicio/fin)
	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

	if (channelName.empty())
	{
		send_numeric(clientFd, "461 TOPIC :Not enough parameters");
		return;
	}

	// existe el canal?
	std::map<std::string, Channel>::iterator it = this->_channels.find(channelName);
	if (it == this->_channels.end())
	{
		// ERR_NOSUCHCHANNEL 403
		send_numeric(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// el emisor está en el canal?
	if (!channel.hasClient(clientFd))
	{
		// ERR_NOTONCHANNEL 442
		send_numeric(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}

	// Construir nick y prefix como en otros comandos
	std::string nick = this->_clients[clientFd].nickname;
	std::string user = this->_clients[clientFd].username;
	if (nick.empty())
		nick = intToString(clientFd);
	if (user.empty())
		user = "user";
	std::string prefix = nick + "!" + user + "@localhost";

	// Si rest está vacío -> petición de ver topic
	if (rest.empty())
	{
		if (channel.hasTopic())
		{
			// RPL_TOPIC 332 : ":server 332 <nick> <channel> :<topic>"
			std::string reply = ":ircserv 332 " + nick + " " + channelName + " :" + channel.getTopic();
			send_numeric(clientFd, reply);
			// (Opcional) podríamos enviar RPL_TOPICWHOTIME (333) con who/time; omitido por simplicidad
		}
		else
		{
			// RPL_NOTOPIC 331
			std::string reply = ":ircserv 331 " + nick + " " + channelName + " :No topic is set";
			send_numeric(clientFd, reply);
		}
		return;
	}

	// Si rest no está vacío -> intento de set topic.
	// El texto del topic normalmente viene tras ':'; si hay ':' al inicio de rest, saltarla.
	std::string newTopic = rest;
	if (!newTopic.empty() && newTopic[0] == ':')
		newTopic.erase(0, 1); // quitar ':'

	// Si el canal tiene topic_restricted y el emisor NO es operador -> error
	if (channel.isTopicRestricted() && !channel.isOperator(clientFd))
	{
		// ERR_CHANOPRIVSNEEDED 482
		send_numeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	// Setear el topic (guardamos también quién lo puso)
	channel.setTopic(newTopic, nick);

	// Notificar a todos los miembros del canal del nuevo topic
	// Mensaje formato: :nick!user@localhost TOPIC <channel> :<topic>
	std::string out = ":" + prefix + " TOPIC " + channelName + " :" + newTopic;

	std::set<int>::iterator sit = channel.clients.begin();
	while (sit != channel.clients.end())
	{
		int fd = *sit;
		if (this->_clients.find(fd) != this->_clients.end())
			send_numeric(fd, out);
		++sit;
	}
}
