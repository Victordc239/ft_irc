/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCommands.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 16:07:46 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/10 16:05:44 by vdiez-cu         ###   ########.fr       */
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
	int clientFd = this->_fds[i].fd;

	if (line.compare(0, 5, "PASS ") == 0)
	{
		std::string given = line.substr(5);

		if (given == this->_serverPassword)
		{
			this->_clients[clientFd].correctPass = true;
			std::cout << "fd " << clientFd << " PASS correcto\n";
		}
		else
		{
			// ERR_PASSWDMISMATCH 464
			sendNumeric(clientFd, "464 :Password incorrect");
			std::cout << "fd " << clientFd << " PASS incorrecto\n";
		}
	}
	else if (line.compare(0, 4, "CAP ") == 0)
	{
		// ignoramos negociación CAP
		sendNumeric(clientFd, "CAP * LS :");
		std::cout << "fd " << clientFd << " CAP recibido (ignorando)\n";
	}

	// NO cerramos nunca aquí
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
	size_t sp = newnick.find(' ');
	if (sp != std::string::npos)
		newnick = newnick.substr(0, sp);

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
		std::string current = this->_clients[clientFd].nickname;
		if (current.empty())
			current = "*";
		std::string err = ":ircserv 433 " + current + " " + newnick + " :Nickname is already in use";
		sendNumeric(clientFd, err);
		return;
	}

	// Si todo bien, asignar el nickname limpio (sin espacios)
	this->_clients[clientFd].nickname = newnick;
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

		size_t sp = user.find(' ');
		if (sp != std::string::npos)
			user = user.substr(0, sp);
	}
	else
	{
		size_t sp = rest.find(' ');
		if (sp == std::string::npos)
			user = rest;
		else
			user = rest.substr(0, sp);
	}

	if (!user.empty())
	{
		this->_clients[clientFd].username = user;
		this->_clients[clientFd].realname = realname;
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
	// Formato esperado: "JOIN <#channel>"
	std::string nameChannel;
	if (line.size() > 5)
		nameChannel = line.substr(5);

	// Trim simple (elimina espacios al inicio/fin) por si el cliente manda cosas raras
	while (!nameChannel.empty() && (nameChannel[0] == ' ' || nameChannel[0] == '\t'))
		nameChannel.erase(0, 1);
	while (!nameChannel.empty() && (nameChannel[nameChannel.size() - 1] == ' ' || nameChannel[nameChannel.size() - 1] == '\t'))
		nameChannel.erase(nameChannel.size() - 1, 1);

	if (nameChannel.empty())
	{
		sendNumeric(clientFd, "461 JOIN :Not enough parameters");
		return;
	}

	/* VALIDACIÓN DEL NOMBRE DEL CANAL: en IRC original solo existen canales que tienen que empezar por
	   uno de estos 4 prefijos, #=canal global, &=canal local, +=canal temporal, !=canal nombre especial */
	if (nameChannel[0] != '#' && nameChannel[0] != '&' && nameChannel[0] != '+' && nameChannel[0] != '!')
	{
		sendNumeric(clientFd, "403 " + nameChannel + " :Invalid channel prefix");
		return;
	}

	// crear canal si no existe; el primer usuario será operador por simplicidad
	if (this->_channels.find(nameChannel) == this->_channels.end())
	{
		Channel newChannel(nameChannel);
		newChannel.addOperator(clientFd);
		this->_channels[nameChannel] = newChannel;
		std::cout << "DEBUG JOIN: creado canal " << nameChannel << " por fd " << clientFd << "\n";
	}

	Channel &channel = this->_channels[nameChannel];

	// si ya está en el canal, ignorar (o podrías enviar 443 ERR_USERONCHANNEL)
	if (!channel.hasClient(clientFd))
	{
		channel.addClient(clientFd);

		/* ===========================================================
		   Construcción del prefijo IRC correcto
		   formato estándar: nick!user@host
		   para ft_irc no necesitamos host real → usamos localhost
		   =========================================================== */

		std::string nick = this->_clients[clientFd].nickname;
		std::string user = this->_clients[clientFd].username;

		if (nick.empty())
			nick = intToString(clientFd);

		if (user.empty())
			user = "user";

		std::string prefix = nick + "!" + user + "@localhost";

		// mensaje JOIN que verán todos los clientes del canal
		std::string joinmsg = ":" + prefix + " JOIN " + nameChannel;

		// Notificar a todos los miembros (incluido el que entra)
		// IMPORTANTE: comprobamos que cada fd aún exista en _clients antes de enviar.
		std::set<int>::iterator iteratorMessageJoin = channel.clients.begin();
		while (iteratorMessageJoin != channel.clients.end())
		{
			int fd = *iteratorMessageJoin;
			if (this->_clients.find(fd) == this->_clients.end())
			{
				std::cout << "DEBUG JOIN: saltando fd " << fd << " (no existe en _clients)\n";
				++iteratorMessageJoin;
				continue;
			}
			std::cout << "DEBUG JOIN: enviando JOIN a fd " << fd << " msg=[" << joinmsg << "]\n";
			sendNumeric(fd, joinmsg);
			++iteratorMessageJoin;
		}

		/* ===========================================================
		   Enviar lista de usuarios del canal al cliente que entra
		   RPL_NAMREPLY (353)
		   =========================================================== */

		std::string names = "353 " + nick + " = " + nameChannel + " :";

		std::set<int>::iterator iteratorCreateList = channel.clients.begin();
		while (iteratorCreateList != channel.clients.end())
		{
			int fd = *iteratorCreateList;
			// Si el cliente ya no existe, lo ignoramos
			if (this->_clients.find(fd) == this->_clients.end())
			{
				++iteratorCreateList;
				continue;
			}

			std::string entryNick;
			if (this->_clients[fd].nickname.empty())
				entryNick = intToString(fd); //si no tiene nickname todavia usar el fd para identificarlo
			else
				entryNick = this->_clients[fd].nickname;

			// si es operador, añadir prefijo '@'
			if (channel.isOperator(fd))
				names += "@" + entryNick + " ";
			else
				names += entryNick + " ";

			++iteratorCreateList;
		}
	
		sendNumeric(clientFd, names);

		/* ===========================================================
		   Fin de lista de nombres
		   RPL_ENDOFNAMES (366)
		   =========================================================== */

		sendNumeric(clientFd, "366 " + nick + " " + nameChannel + " :End of /NAMES list");

		std::cout << "DEBUG JOIN: cliente fd " << clientFd << " unido a " << nameChannel << "\n";
	}
	else
	{
		std::cout << "DEBUG JOIN: fd " << clientFd << " ya estaba en " << nameChannel << "\n";
	}
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
	size_t sp = line.find(' ', prefix_len);
	std::string channelName;
	std::string reason;

	if (sp == std::string::npos)
	{
		// puede que línea sea "PART #channelName" o "PART #channelName\r"
		channelName = line.substr(prefix_len);
		// quitar CR final si existe
		if (!channelName.empty() && channelName[channelName.size() - 1] == '\r')
			channelName.erase(channelName.size() - 1, 1);
	}
	else
	{
		channelName = line.substr(prefix_len, sp - prefix_len);
		// buscar si hay ':' para reason después de sp
		size_t colon = line.find(':', sp + 1);
		if (colon != std::string::npos)
		{
			reason = line.substr(colon + 1);
			if (!reason.empty() && reason[reason.size() - 1] == '\r')
				reason.erase(reason.size() - 1, 1);
		}
		else
		{
			// tal vez no haya comment: el resto es el canal o espacios
			std::string maybe = line.substr(sp + 1);
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
	std::map<std::string, Channel>::iterator it = this->_channels.find(channelName);
	if (it == this->_channels.end())
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
	std::string emNick = this->_clients[clientFd].nickname;
	std::string emUser = this->_clients[clientFd].username;
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
		if (this->_clients.find(fd) != this->_clients.end())
			sendNumeric(fd, out);
		++sit;
	}

	// Quitar al cliente del canal (Channel::removeClient también quita operadores)
	channel.removeClient(clientFd);

	// Si el canal queda vacío, eliminarlo del mapa
	if (channel.clients.empty())
		this->_channels.erase(it);
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

	size_t sp = line.find(' ', prefix_len);
	if (sp == std::string::npos)
	{
		sendNumeric(clientFd, "461 PRIVMSG :Not enough parameters");
		return;
	}

	std::string target = line.substr(prefix_len, sp - prefix_len);

	// Trim target por si acaso
	while (!target.empty() && (target[0] == ' ' || target[0] == '\t'))
		target.erase(0, 1);
	while (!target.empty() && (target[target.size() - 1] == ' ' || target[target.size() - 1] == '\t'))
		target.erase(target.size() - 1, 1);

	// message puede empezar con ':' después del espacio; buscamos ':' tras el target
	size_t colon = line.find(':', sp + 1);
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

	std::string nick = this->_clients[clientFd].nickname;
	std::string user = this->_clients[clientFd].username;

	if (nick.empty())
		nick = intToString(clientFd);

	if (user.empty())
		user = "user";

	std::string prefix = nick + "!" + user + "@localhost";

	// Mensaje a canal
	if (!target.empty() && (target[0] == '#' || target[0] == '&' || target[0] == '+' || target[0] == '!'))
	{
		// existe el canal?
		std::map<std::string, Channel>::iterator channelIterator = this->_channels.find(target);
		if (channelIterator == this->_channels.end())
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

		for (std::set<int>::iterator it = channel.clients.begin(); it != channel.clients.end(); ++it)
		{
			int fd = *it;
			// evitar enviar al emisor
			if (fd == clientFd)
				continue;
			// comprueba que el fd sigue en la tabla de clientes
			if (this->_clients.find(fd) == this->_clients.end())
			{
				std::cout << "DEBUG PRIVMSG: saltando fd " << fd << " (no existe en _clients)\n";
				continue;
			}

			std::cout << "DEBUG PRIVMSG: reenviando PRIVMSG de fd " << clientFd << " a fd " << fd << " msg=[" << out << "]\n";
			sendNumeric(fd, out);
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

		std::cout << "DEBUG PRIVMSG: enviando PRIVMSG de fd " << clientFd << " a fd " << dst_fd << " msg=[" << out << "]\n";
		sendNumeric(dst_fd, out);
	}
}
