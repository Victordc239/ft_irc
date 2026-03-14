/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerOperatorsCommands.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/09 14:39:18 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/13 10:55:13 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

void	Server::handleKickCommand(int clientFd, const std::string &line)
{
	// "KICK " tiene longitud 5
	const size_t prefix_len = 5;
	if (line.size() <= prefix_len)
	{
		sendNumeric(clientFd, "461 KICK :Not enough parameters");
		return;
	}

	// extraer canal (primer token tras "KICK ")
	size_t space = line.find(' ', prefix_len);
	if (space == std::string::npos)
	{
		sendNumeric(clientFd, "461 KICK :Not enough parameters");
		return;
	}
	std::string channelName = line.substr(prefix_len, space - prefix_len);

	// trim simple de channelName (inicio/fin)
	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

	// extraer nick objetivo y comentario opcional
	std::string targetNick;
	std::string comment;

	// buscar ':' que indica inicio de comment
	size_t colon = line.find(':', space + 1);

	if (colon == std::string::npos)
	{
		// no hay comment, el nick va hasta el final
		targetNick = line.substr(space + 1);
		// trim final CR si hay
		if (!targetNick.empty() && targetNick[targetNick.size() - 1] == '\r')
			targetNick.erase(targetNick.size() - 1, 1);
	}
	else
	{
		// nick está entre space+1 y colon-1
		if (colon > space + 1)
			targetNick = line.substr(space + 1, colon - (space + 1));
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
		sendNumeric(clientFd, "461 KICK :Not enough parameters");
		return;
	}

	// ver si existe el canal
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		sendNumeric(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// el emisor está en el canal?
	if (!channel.hasClient(clientFd))
	{
		sendNumeric(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}

	// el emisor es operador?
	if (!channel.isOperator(clientFd))
	{
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	// existe el nick objetivo en el servidor?
	int targetFd = getFdByNick(targetNick);
	if (targetFd == -1)
	{
		sendNumeric(clientFd, "401 " + targetNick + " :No such nick");
		return;
	}

	// el objetivo está en el canal?
	if (!channel.hasClient(targetFd))
	{
		sendNumeric(clientFd, "441 " + targetNick + " " + channelName + " :They aren't on that channel");
		return;
	}

	// Construir prefijo: nick!user@localhost (igual que en otros comandos)
	std::string emNick = _clients[clientFd].nickname;
	std::string emUser = _clients[clientFd].username;
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
		if (_clients.find(fd) != _clients.end())
			sendNumeric(fd, out);
		++sit;
	}

	// Quitar al cliente del canal (y si era operador, Channel::removeClient ya lo quita de operators)
	channel.removeClient(targetFd);

	// Si el canal queda vacío, eliminarlo del mapa
	if (channel.clients.empty())
		_channels.erase(it);
}

void	Server::handleInviteCommand(int clientFd, const std::string &line)
{
	// "INVITE " tiene longitud 7
	const size_t prefix_len = 7;
	if (line.size() <= prefix_len)
	{
		sendNumeric(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	// extraer nick objetivo (primer token tras "INVITE ")
	size_t space = line.find(' ', prefix_len);
	if (space == std::string::npos)
	{
		sendNumeric(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	std::string targetNick = line.substr(prefix_len, space - prefix_len);

	// el resto es el canal (aceptamos "INVITE nick #channelName" y también "INVITE nick :#channelName")
	std::string channelName;
	size_t channelNameStart = space + 1;
	if (channelNameStart >= line.size())
	{
		sendNumeric(clientFd, "461 INVITE :Not enough parameters");
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
		sendNumeric(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	// comprobar que existe el nick objetivo en el servidor
	int targetFd = getFdByNick(targetNick);
	if (targetFd == -1)
	{
		sendNumeric(clientFd, "401 " + targetNick + " :No such nick");
		return;
	}

	// comprobar que existe el canal
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		sendNumeric(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// el emisor está en el canal?
	if (!channel.hasClient(clientFd))
	{
		sendNumeric(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}

	// SOLO operadores pueden INVITE por defecto
	if (!channel.isOperator(clientFd))
	{
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	// el objetivo ya está en el canal?
	if (channel.hasClient(targetFd))
	{
		// ERR_USERONCHANNEL 443
		sendNumeric(clientFd, "443 " + targetNick + " " + channelName + " :is already on channel");
		return;
	}

	// Construir prefijo: nick!user@localhost
	std::string emNick = _clients[clientFd].nickname;
	std::string emUser = _clients[clientFd].username;
	if (emNick.empty())
		emNick = intToString(clientFd);
	if (emUser.empty())
		emUser = "user";
	std::string prefix = emNick + "!" + emUser + "@localhost";

	// Mensaje INVITE que recibe el usuario invitado
	std::string inviteMsg = ":" + prefix + " INVITE " + targetNick + " " + channelName;
	sendNumeric(targetFd, inviteMsg);

	// Añadir invitación efectiva en la lista de invitados del canal
	channel.addInvite(targetFd);

	// Notificar al emisor con RPL_INVITING (341)
	sendNumeric(clientFd, "341 " + emNick + " " + targetNick + " " + channelName);

	std::cout << "INVITE: fd " << clientFd << " invitó a " << targetNick << " a " << channelName << "\n";
}

void Server::handleTopicCommand(int clientFd, const std::string &line)
{
	// Formatos posibles:
	// "TOPIC <#channel>"              -> ver topic
	// "TOPIC <#channel> :<new topic>" -> cambiar topic (el ':' puede ser obligatorio en clientes)
	const size_t prefix_len = 6; // strlen("TOPIC ")
	if (line.size() <= prefix_len)
	{
		sendNumeric(clientFd, "461 TOPIC :Not enough parameters");
		return;
	}

	// extraer canal
	size_t space = line.find(' ', prefix_len);
	std::string channelName;
	std::string rest;
	if (space == std::string::npos)
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
		channelName = line.substr(prefix_len, space - prefix_len);
		rest = line.substr(space + 1);
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
		sendNumeric(clientFd, "461 TOPIC :Not enough parameters");
		return;
	}

	// existe el canal?
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		// ERR_NOSUCHCHANNEL 403
		sendNumeric(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// el emisor está en el canal?
	if (!channel.hasClient(clientFd))
	{
		// ERR_NOTONCHANNEL 442
		sendNumeric(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}

	// Construir nick y prefix como en otros comandos
	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;
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
			sendNumeric(clientFd, reply);
			// (Opcional) podríamos enviar RPL_TOPICWHOTIME (333) con who/time; omitido por simplicidad
		}
		else
		{
			// RPL_NOTOPIC 331
			std::string reply = ":ircserv 331 " + nick + " " + channelName + " :No topic is set";
			sendNumeric(clientFd, reply);
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
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
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
		if (_clients.find(fd) != _clients.end())
			sendNumeric(fd, out);
		++sit;
	}
}

void Server::handleModeCommand(int clientFd, const std::string &line)
{
	const size_t prefix_len = 5; // "MODE "
	if (line.size() <= prefix_len)
	{
		sendNumeric(clientFd, "461 MODE :Not enough parameters");
		return;
	}

	// Extraer channelName y resto (rest puede contener modes + params)
	size_t space = line.find(' ', prefix_len);
	std::string channelName;
	std::string rest;
	if (space == std::string::npos)
	{
		channelName = line.substr(prefix_len);
		if (!channelName.empty() && channelName[channelName.size() - 1] == '\r')
			channelName.erase(channelName.size() - 1, 1);
		rest = "";
	}
	else
	{
		channelName = line.substr(prefix_len, space - prefix_len);
		rest = line.substr(space + 1);
		if (!rest.empty() && rest[rest.size() - 1] == '\r')
			rest.erase(rest.size() - 1, 1);
	}

	// Trim sencillo de channelName (inicio/fin)
	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

	if (channelName.empty())
	{
		sendNumeric(clientFd, "461 MODE :Not enough parameters");
		return;
	}

	/* Si el target no empieza con un caracter de canal entonces es un usuario
	   y lo manejamos para modos de usuario (ej. MODE <nick> +i) */
	if (channelName[0] != '#' && channelName[0] != '&' && channelName[0] != '+' && channelName[0] != '!')
	{
		mode_user(clientFd, channelName, rest);
		return;
	}

	// existe el canal?
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		sendNumeric(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// emisor en canal?
	if (!channel.hasClient(clientFd))
	{
		sendNumeric(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}

	// Construir nick/usser/prefix (los necesitamos para las respuestas)
	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;
	if (nick.empty())
		nick = intToString(clientFd);
	if (user.empty())
		user = "user";
	std::string prefix = nick + "!" + user + "@localhost";

	// ------------------------------
	// Rama de LECTURA de modos (QUERY)
	// Si rest está vacío -> petición de ver modos actuales (RPL_CHANNELMODEIS 324)
	// Permitimos esta consulta a cualquier miembro del canal.
	// ------------------------------
	if (rest.empty())
	{
		std::string modes = "";
		std::string params = "";
		if (channel.isInviteOnly())
			modes += "i";
		if (channel.isTopicRestricted())
			modes += "t";
		if (channel.hasKey())
		{
			modes += "k";
			params += " " + channel.getKey();
		}
		if (channel.getLimit() > 0)
		{
			modes += "l";
			params += " " + intToString(channel.getLimit());
		}
		std::string reply = ":ircserv 324 " + nick + " " + channelName + " +" + modes + params;
		sendNumeric(clientFd, reply);
		return;
	}

	// ------------------------------
	// A partir de aquí -> se intenta CAMBIAR modos (rest NO está vacío)
	// Por seguridad/consistencia: solo operadores pueden cambiar modos.
	// ------------------------------
	if (!channel.isOperator(clientFd))
	{
		sendNumeric(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	// Tokenizar rest *manualmente* (sin usar split) — tokens separados por espacios
	std::vector<std::string> tokens;
	std::string s = rest;
	while (!s.empty())
	{
		// saltar espacios iniciales
		while (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
			s.erase(0, 1);
		if (s.empty())
			break;
		size_t p = s.find(' ');
		if (p == std::string::npos)
		{
			tokens.push_back(s);
			break;
		}
		else
		{
			tokens.push_back(s.substr(0, p));
			s.erase(0, p + 1);
		}
	}

	if (tokens.empty())
	{
		sendNumeric(clientFd, "461 MODE :Not enough parameters");
		return;
	}

	std::string modeToken = tokens[0]; // ej "+itk"
	size_t paramIndex = 1;
	bool plus = true;

	size_t i = 0;
	while (i < modeToken.size())
	{
		if (modeToken[i] == '+')
		{
			plus = true;
			++i;
			continue;
		}

		if (modeToken[i] == '-')
		{
			plus = false;
			++i;
			continue;
		}

		// Para los modos que requieren parámetro, obtenerlo desde tokens[paramIndex]
		if (modeToken[i] == 'i')
			mode_i(clientFd, channel, channelName, plus, prefix);
		else if (modeToken[i] == 't')
			mode_t(clientFd, channel, channelName, plus, prefix);
		else if (modeToken[i] == 'k')
		{
			if (plus)
			{
				if (paramIndex >= tokens.size())
				{
					sendNumeric(clientFd, "461 MODE :Not enough parameters");
					return;
				}
				mode_k(clientFd, channel, channelName, plus, tokens[paramIndex++], prefix);
			}
			else // -k no necesita parámetro
				mode_k(clientFd, channel, channelName, plus, std::string(""), prefix);
		}
		else if (modeToken[i] == 'l')
		{
			if (plus)
			{
				if (paramIndex >= tokens.size())
				{
					sendNumeric(clientFd, "461 MODE :Not enough parameters");
					return;
				}
				mode_l(clientFd, channel, channelName, plus, tokens[paramIndex++], prefix);
			}
			else // -l no necesita parámetro para quitar el límite
				mode_l(clientFd, channel, channelName, plus, std::string(""), prefix);
		}
		else if (modeToken[i] == 'o')
		{
			// necesita parámetro nick
			if (paramIndex >= tokens.size())
			{
				sendNumeric(clientFd, "461 MODE :Not enough parameters");
				return;
			}
			mode_o(clientFd, channel, channelName, plus, tokens[paramIndex++], prefix);
		}
		else
		{
			// modo desconocido -> RPL_UNKNOWNMODE (usamos 472 como en implementaciones simples)
			std::string unknown = ":ircserv 472 " + nick + " " + std::string(1, modeToken[i]) + " :is unknown mode char to me";
			sendNumeric(clientFd, unknown);
		}
		++i;
	}
}
