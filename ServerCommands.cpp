/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCommands.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 16:07:46 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/12 16:41:45 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

/* ===========================================================
   FASE INICIAL DE AUTENTICACIÓN
   Ahora NO cerramos conexión si el orden es incorrecto.
   Permitimos PASS en cualquier momento antes del registro.
   Si es incorrecto enviamos 464 pero NO cerramos.
   CAP se ignora.
   =========================================================== */
bool Server::handleInitialAuthentication(size_t &i, const std::string &line)
{
	int clientFd = _fds[i].fd;

	if (line.compare(0, 5, "PASS ") == 0)
	{
		std::string given = line.substr(5);

		if (given == _serverPassword)
		{
			_clients[clientFd].correctPass = true;
			std::cout << "fd " << clientFd << " PASS correcto\n";
		}
		else
		{
			sendNumeric(clientFd, "464 :Password incorrect");
			std::cout << "fd " << clientFd << " PASS incorrecto\n";
		}
	}
	else if (line.compare(0, 4, "CAP ") == 0)
	{
		// parse sencillo del subcomando tras "CAP "
		std::string rest = line.substr(4);

		// trim inicio (espacios/tabs)
		size_t start = 0;
		while (start < rest.size() && (rest[start] == ' ' || rest[start] == '\t'))
			start++;

		rest = rest.substr(start);

		// obtener primer token (subcomando)
		size_t space = rest.find(' ');
		std::string subcmd;

		if (space == std::string::npos)
			subcmd = rest;
		else
			subcmd = rest.substr(0, space);

		// quitar CR si llegó
		if (!subcmd.empty() && subcmd[subcmd.size() - 1] == '\r')
			subcmd.erase(subcmd.size() - 1);

		if (subcmd == "LS")
		{
			// Respondemos exactamente como hace irssi por defecto cuando pedimos LS:
			sendNumeric(clientFd, "CAP * LS :");
			std::cout << "fd " << clientFd << " CAP LS recibido (ignorando)\n";
		}
		else if (subcmd == "END")
		{
			// No respondemos nada, solo lo ignoramos
			std::cout << "fd " << clientFd << " CAP END recibido (ignorando)\n";
		}
		else
		{
			// Otros subcomandos CAP: ignorar pero loguear
			std::cout << "fd " << clientFd << " CAP " << subcmd << " recibido (ignorando)\n";
		}
	}
	return false;
}

void Server::handleNickCommand(int clientFd, const std::string &line)
{
	// Extraer argumento tras "NICK "
	std::string newnick;
	if (line.size() > 5)
		newnick = line.substr(5);

	// Trim simple: quitar espacios/tabs al inicio/fin
	size_t start = 0;
	while (start < newnick.size() && (newnick[start] == ' ' || newnick[start] == '\t'))
		++start;
	size_t end = newnick.size();
	while (end > start && (newnick[end - 1] == ' ' || newnick[end - 1] == '\t'))
		--end;
	newnick = newnick.substr(start, end - start);

	// Si el cliente envía más de un token (ej: "nick extra words"),
	// tomar sólo el primer token (hasta el primer espacio).
	size_t space = newnick.find(' ');
	if (space != std::string::npos)
		newnick = newnick.substr(0, space);

	// limpiar CR si por alguna razón llegó
	if (!newnick.empty() && newnick[newnick.size() - 1] == '\r')
		newnick.erase(newnick.size() - 1);

	// validar
	if (newnick.empty())
	{
		// ERR_NONICKNAMEGIVEN 431 (sin prefijo de servidor para simplificar)
		sendNumeric(clientFd, "431 :No nickname given");
		return;
	}

	if (nickInUse(newnick))
	{
		/* ERR_NICKNAMEINUSE 433
		   Formato recomendado: :<servername> 433 <yournick-or-*> <attemptednick> :Nickname is already in use
		   Usamos "ircserv" como servername por ahora. */
		std::string current = _clients[clientFd].nickname;
		if (current.empty())
			current = "*";
		std::string err = ":ircserv 433 " + current + " " + newnick + " :Nickname is already in use";
		sendNumeric(clientFd, err);
		return;
	}

	// Si todo bien, asignar el nickname limpio (sin espacios)
	_clients[clientFd].nickname = newnick;
	std::cout << "fd " << clientFd << " set NICK=" << newnick << "\n";
}

void Server::handleUserCommand(int clientFd, const std::string &line)
{
	// FORMATO: USER <user> <mode> <unused> :<realname>
	std::string rest = line.substr(5);
	std::string user;
	std::string realname;

	size_t posColon = rest.find(" :");

	if (posColon != std::string::npos)
	{
		realname = rest.substr(posColon + 2);
		user = rest.substr(0, posColon);

		size_t space = user.find(' ');
		if (space != std::string::npos)
			user = user.substr(0, space);
	}
	else
	{
		size_t space = rest.find(' ');
		if (space == std::string::npos)
			user = rest;
		else
			user = rest.substr(0, space);
	}

	if (!user.empty())
	{
		_clients[clientFd].username = user;
		_clients[clientFd].realname = realname;
		std::cout << "fd " << clientFd << " set USER=" << user << " REAL=" << realname << "\n";
	}
	else
	{
		// ERR_NEEDMOREPARAMS 461 (usar sendNumeric en vez de send directo)
		sendNumeric(clientFd, "461 USER :Not enough parameters");
	}
}

void Server::handleJoinCommand(int clientFd, const std::string &line)
{
	// Formato esperado: "JOIN <#channel> [key]"
	std::string rest;
	if (line.size() > 5)
		rest = line.substr(5);

	// trim inicio/fin
	while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t'))
		rest.erase(0, 1);
	while (!rest.empty() && (rest[rest.size() - 1] == ' ' || rest[rest.size() - 1] == '\t'))
		rest.erase(rest.size() - 1, 1);
	if (rest.empty())
	{
		sendNumeric(clientFd, "461 JOIN :Not enough parameters");
		return;
	}

	// separar canal y key opcional (solo soportamos un canal por JOIN en esta impl)
	std::string nameChannel;
	std::string joinKey;
	size_t space = rest.find(' ');
	if (space == std::string::npos)
		nameChannel = rest;
	else
	{
		nameChannel = rest.substr(0, space);
		joinKey = rest.substr(space + 1);
		// trim joinKey
		while (!joinKey.empty() && (joinKey[0] == ' ' || joinKey[0] == '\t'))
			joinKey.erase(0, 1);
		while (!joinKey.empty() && (joinKey[joinKey.size() - 1] == ' ' || joinKey[joinKey.size() - 1] == '\t'))
			joinKey.erase(joinKey.size() - 1, 1);
	}

	// quitar CR si hay
	if (!nameChannel.empty() && nameChannel[nameChannel.size() - 1] == '\r')
		nameChannel.erase(nameChannel.size() - 1, 1);
	if (!joinKey.empty() && joinKey[joinKey.size() - 1] == '\r')
		joinKey.erase(joinKey.size() - 1, 1);

	if (nameChannel.empty())
	{
		sendNumeric(clientFd, "461 JOIN :Not enough parameters");
		return;
	}

	/* VALIDACIÓN DEL NOMBRE DEL CANAL */
	if (nameChannel[0] != '#' && nameChannel[0] != '&' && nameChannel[0] != '+' && nameChannel[0] != '!')
	{
		sendNumeric(clientFd, "403 " + nameChannel + " :Invalid channel prefix");
		return;
	}

	// crear canal si no existe; el primer usuario será operador por simplicidad
	if (_channels.find(nameChannel) == _channels.end())
	{
		Channel newChannel(nameChannel);
		newChannel.addOperator(clientFd);
		_channels[nameChannel] = newChannel;
	}

	Channel &channel = _channels[nameChannel];

	// si ya está en el canal, ignorar (o podrías enviar 443 ERR_USERONCHANNEL)
	if (channel.hasClient(clientFd))
	{
		std::cout << "JOIN: fd " << clientFd << " ya estaba en " << nameChannel << "\n";
		return;
	}

	// === CHECK: invite-only (+i) ===
	if (channel.isInviteOnly() && !channel.isInvited(clientFd))
	{
		// ERR_INVITEONLYCHAN 473
		std::string nick = _clients[clientFd].nickname;
		if (nick.empty()) nick = intToString(clientFd);
		sendNumeric(clientFd, "473 " + nick + " " + nameChannel + " :Cannot join channel (+i)");
		return;
	}

	// === CHECK: key (+k) ===
	if (channel.hasKey())
	{
		// si no ha proporcionado key o es incorrecta -> ERR_BADCHANNELKEY 475
		if (joinKey.empty() || joinKey != channel.getKey())
		{
			std::string nick = _clients[clientFd].nickname;
			if (nick.empty()) nick = intToString(clientFd);
			sendNumeric(clientFd, "475 " + nick + " " + nameChannel + " :Cannot join channel (+k)");
			return;
		}
	}

	// === CHECK: limit (+l) ===
	if (channel.getLimit() > 0 && (int)channel.clients.size() >= channel.getLimit())
	{
		std::string nick = _clients[clientFd].nickname;
		if (nick.empty()) nick = intToString(clientFd);
		sendNumeric(clientFd, "471 " + nick + " " + nameChannel + " :Cannot join channel (+l)");
		return;
	}

	// Todo OK -> unir al canal
	channel.addClient(clientFd);

	// Si venía por invitación, consumirla (la invitación se gasta)
	if (channel.isInvited(clientFd))
		channel.removeInvite(clientFd);

	/* ===========================================================
	   Construcción del prefijo IRC correcto (nick!user@localhost)
	   =========================================================== */

	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;

	if (nick.empty())
		nick = intToString(clientFd);

	if (user.empty())
		user = "user";

	std::string prefix = nick + "!" + user + "@localhost";

	// mensaje JOIN que verán todos los clientes del canal
	std::string joinmsg = ":" + prefix + " JOIN " + nameChannel;

	// Notificar a todos los miembros (incluido el que entra)
	std::set<int>::iterator iteratorMessageJoin = channel.clients.begin();
	while (iteratorMessageJoin != channel.clients.end())
	{
		int fd = *iteratorMessageJoin;
		if (_clients.find(fd) == _clients.end())
		{
			++iteratorMessageJoin;
			continue;
		}
		sendNumeric(fd, joinmsg);
		++iteratorMessageJoin;
	}

	/* Enviar lista de usuarios del canal al cliente que entra (NAMES) */
	std::string names = "353 " + nick + " = " + nameChannel + " :";

	std::set<int>::iterator iteratorCreateList = channel.clients.begin();
	while (iteratorCreateList != channel.clients.end())
	{
		int fd = *iteratorCreateList;
		if (_clients.find(fd) == _clients.end())
		{
			++iteratorCreateList;
			continue;
		}

		std::string entryNick;
		if (_clients[fd].nickname.empty())
			entryNick = intToString(fd);
		else
			entryNick = _clients[fd].nickname;

		if (channel.isOperator(fd))
			names += "@" + entryNick + " ";
		else
			names += entryNick + " ";

		++iteratorCreateList;
	}

	sendNumeric(clientFd, names);
	sendNumeric(clientFd, "366 " + nick + " " + nameChannel + " :End of /NAMES list");

	std::cout << "JOIN: cliente fd " << clientFd << " unido a " << nameChannel << "\n";
}

void Server::handlePartCommand(int clientFd, const std::string &line)
{
	// Prefijo "PART " longitud 5
	const size_t prefix_len = 5;
	if (line.size() <= prefix_len)
	{
		sendNumeric(clientFd, "461 PART :Not enough parameters");
		return;
	}

	// extraer canal (soportamos sólo un canal en esta implementación)
	size_t space = line.find(' ', prefix_len);
	std::string channelName;
	std::string reason;

	if (space == std::string::npos)
	{
		// puede que línea sea "PART #channelName" o "PART #channelName\r"
		channelName = line.substr(prefix_len);
		// quitar CR final si existe
		if (!channelName.empty() && channelName[channelName.size() - 1] == '\r')
			channelName.erase(channelName.size() - 1, 1);
	}
	else
	{
		channelName = line.substr(prefix_len, space - prefix_len);
		// buscar si hay ':' para reason después de space
		size_t colon = line.find(':', space + 1);
		if (colon != std::string::npos)
		{
			reason = line.substr(colon + 1);
			if (!reason.empty() && reason[reason.size() - 1] == '\r')
				reason.erase(reason.size() - 1, 1);
		}
		else
		{
			// tal vez no haya comment: el resto es el canal o espacios
			std::string maybe = line.substr(space + 1);
			// si no contiene ':', no lo usamos como reason; normalmente after channelName there is optional reason introduced by ':'
			(void)maybe;
		}
	}

	// trim sencillo de channelName (inicio/fin)
	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);

	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

	if (channelName.empty())
	{
		sendNumeric(clientFd, "461 PART :Not enough parameters");
		return;
	}

	// Existe el canal?
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		sendNumeric(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	// Comprueba que el emisor esté en el canal
	if (!channel.hasClient(clientFd))
	{
		sendNumeric(clientFd, "442 " + channelName + " :You're not on that channel");
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

	// Construir mensaje PART
	std::string out = ":" + prefix + " PART " + channelName;
	if (!reason.empty())
		out += " :" + reason;
	else
		out += " :";

	// Enviar PART a todos los miembros del canal (incluyendo el que sale)
	std::set<int>::iterator sit = channel.clients.begin();
	while (sit != channel.clients.end())
	{
		int fd = *sit;
		if (_clients.find(fd) != _clients.end())
			sendNumeric(fd, out);
		++sit;
	}

	// Quitar al cliente del canal (Channel::removeClient también quita operadores)
	channel.removeClient(clientFd);

	// Si el canal queda vacío, eliminarlo del mapa
	if (channel.clients.empty())
		_channels.erase(it);
}

void Server::handlePrivmsgCommand(int clientFd, const std::string &line)
{
	// Formato: "PRIVMSG <target> :<message>"
	const size_t prefix_len = 8; // strlen("PRIVMSG ")
	if (line.size() <= prefix_len)
	{
		sendNumeric(clientFd, "461 PRIVMSG :Not enough parameters");
		return;
	}

	size_t space = line.find(' ', prefix_len);
	if (space == std::string::npos)
	{
		sendNumeric(clientFd, "461 PRIVMSG :Not enough parameters");
		return;
	}

	std::string target = line.substr(prefix_len, space - prefix_len);

	// Trim target por si acaso
	while (!target.empty() && (target[0] == ' ' || target[0] == '\t'))
		target.erase(0, 1);
	while (!target.empty() && (target[target.size() - 1] == ' ' || target[target.size() - 1] == '\t'))
		target.erase(target.size() - 1, 1);

	// message puede empezar con ':' después del espacio; buscamos ':' tras el target
	size_t colon = line.find(':', space + 1);
	std::string text;

	if (colon == std::string::npos)
	{
		// No hay ':' — no message
		sendNumeric(clientFd, "412 :No text to send");
		return;
	}
	else
	{
		text = line.substr(colon + 1);
		if (text.empty())
		{
			sendNumeric(clientFd, "412 :No text to send");
			return;
		}
	}

	/* ===========================================================
	   Construcción del prefijo IRC correcto
	   =========================================================== */

	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;

	if (nick.empty())
		nick = intToString(clientFd);

	if (user.empty())
		user = "user";

	std::string prefix = nick + "!" + user + "@localhost";

	// Mensaje a canal
	if (!target.empty() && (target[0] == '#' || target[0] == '&' || target[0] == '+' || target[0] == '!'))
	{
		// existe el canal?
		std::map<std::string, Channel>::iterator channelIterator = _channels.find(target);
		if (channelIterator == _channels.end())
		{
			sendNumeric(clientFd, "403 " + target + " :No such channel");
			return;
		}

		Channel &channel = channelIterator->second;

		// el cliente está en el canal?
		if (!channel.hasClient(clientFd))
		{
			sendNumeric(clientFd, "442 " + target + " :You're not on that channel");
			return;
		}

		// reenviar a todos los clientes del canal (excepto el emisor)
		std::string out = ":" + prefix + " PRIVMSG " + target + " :" + text;

		std::set<int>::iterator it = channel.clients.begin();
		while (it != channel.clients.end())
		{
			int fd = *it;
			// evitar enviar al emisor
			if (fd == clientFd)
			{
				++it;
				continue;
			}
			// comprueba que el fd sigue en la tabla de clientes
			if (_clients.find(fd) == _clients.end())
			{
				++it;
				continue;
			}
			sendNumeric(fd, out);
			++it;
		}
	}
	else
	{
		// Mensaje privado a nick
		int dst_fd = getFdByNick(target);

		if (dst_fd == -1)
		{
			sendNumeric(clientFd, "401 " + target + " :No such nick");
			return;
		}

		std::string out = ":" + prefix + " PRIVMSG " + target + " :" + text;

		std::cout << "PRIVMSG: enviando PRIVMSG de fd " << clientFd << " a fd " << dst_fd << " msg=[" << out << "]\n";
		sendNumeric(dst_fd, out);
	}
}
