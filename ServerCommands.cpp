/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCommands.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 16:07:46 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/27 17:17:14 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

// FASE INICIAL DE AUTENTICACIÓN. Procesa comandos iniciales de autenticación (PASS y CAP) y marca si la contraseña es correcta, ignorando CAP.
// Ahora NO cerramos conexión si el orden es incorrecto. Permitimos PASS en cualquier momento antes del registro.
// Si es incorrecto enviamos 464 pero NO cerramos. CAP se ignora.
bool Server::handleAuthenticationCmds(size_t &i, const std::string &line)
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
			sendNumericMessage(clientFd, "464 :Password incorrect");
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
			sendNumericMessage(clientFd, "CAP * LS :"); // Respondemos exactamente como hace irssi por defecto cuando pedimos LS:
			std::cout << "fd " << clientFd << " CAP LS recibido (ignorando)\n";
		}
		else if (subcmd == "END")
			std::cout << "fd " << clientFd << " CAP END recibido (ignorando)\n"; // No respondemos nada, solo lo ignoramos
		else
			std::cout << "fd " << clientFd << " CAP " << subcmd << " recibido (ignorando)\n"; // Otros subcomandos CAP: ignorar pero loguear
	}
	return (false);
}

// Asigna un nickname a un cliente verificando que no esté en uso y enviando error si falla
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
		sendNumericMessage(clientFd, "431 :No nickname given");
		return;
	}

	if (isNickInUse(newnick))
	{
		// ERR_NICKNAMEINUSE 433
		//    Usamos "ircserv" como servername por ahora.
		std::string current = _clients[clientFd].nickname;
		if (current.empty())
			current = "*";
		std::string err = ":ircserv 433 " + current + " " + newnick + " :Nickname is already in use";
		sendNumericMessage(clientFd, err);
		return;
	}

	// Si todo bien, asignar el nickname limpio (sin espacios)
	_clients[clientFd].nickname = newnick;
	std::cout << "fd " << clientFd << " set NICK=" << newnick << "\n";
}

// Registra el nombre de usuario del cliente con el comando USER
void Server::handleUserCommand(int clientFd, const std::string &line)
{
	// USER <user> <mode> <unused> :<realname>
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
		std::cout << "fd " << clientFd << " set USER=" << user << "\n";
	}
	else
		sendNumericMessage(clientFd, "461 USER :Not enough parameters"); 	// ERR_NEEDMOREPARAMS 461 (usar sendNumericMessage en vez de send directo)
}

// Permite a un cliente unirse a un canal, creando el canal si no existe, validando permisos y claves
void Server::handleJoinCommand(int clientFd, const std::string &line)
{
	// "JOIN <#channel> [key]"
	std::string rest;
	if (line.size() > 5)
		rest = line.substr(5);

	// trim inicio/fin
	while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t'))
		rest.erase(0, 1);
	while (!rest.empty() && (rest[rest.size() - 1] == ' ' || rest[rest.size() - 1] == '\t'))
		rest.erase(rest.size() - 1, 1);

	// Si no hay parámetros, devolvemos error estándar
	if (rest.empty())
	{
		sendNumericMessage(clientFd, "461 JOIN :Not enough parameters");
		return;
	}

	// separar canal y key opcional (solo soportamos un canal por JOIN en esta impl)
	std::string nameChannel;
	std::string joinKey;
	size_t space = rest.find(' ');
	if (space == std::string::npos)
	{
		nameChannel = rest;
	}
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

	// Validación mínima: nombre de canal no vacío
	if (nameChannel.empty())
	{
		sendNumericMessage(clientFd, "461 JOIN :Not enough parameters");
		return;
	}

	// VALIDACIÓN DEL NOMBRE DEL CANAL: prefijo válido (# & + !)
	if (nameChannel[0] != '#' && nameChannel[0] != '&' && nameChannel[0] != '+' && nameChannel[0] != '!')
	{
		sendNumericMessage(clientFd, "403 " + nameChannel + " :Invalid channel prefix");
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
	if (channel.isClientInChannel(clientFd))
	{
		std::cout << "JOIN: fd " << clientFd << " ya estaba en " << nameChannel << "\n";
		return;
	}

	// === CHECK: invite-only (+i) ===
	if (channel.isInviteOnly() && !channel.isClientInvited(clientFd))
	{
		// ERR_INVITEONLYCHAN 473
		std::string nick = _clients[clientFd].nickname;
		if (nick.empty())
			nick = convertIntToString(clientFd);
		sendNumericMessage(clientFd, "473 " + nick + " " + nameChannel + " :Cannot join channel (+i)");
		return;
	}

	// === CHECK: key (+k) ===
	if (channel.hasKey())
	{
		// si no ha proporcionado key o es incorrecta -> ERR_BADCHANNELKEY 475
		if (joinKey.empty() || joinKey != channel.getKey())
		{
			std::string nick = _clients[clientFd].nickname;
			if (nick.empty())
				nick = convertIntToString(clientFd);
			sendNumericMessage(clientFd, "475 " + nick + " " + nameChannel + " :Cannot join channel (+k)");
			return;
		}
	}

	// === CHECK: limit (+l) ===
	if (channel.getUserLimit() > 0 && (int)channel.clients.size() >= channel.getUserLimit())
	{
		std::string nick = _clients[clientFd].nickname;
		if (nick.empty())
			nick = convertIntToString(clientFd);
		sendNumericMessage(clientFd, "471 " + nick + " " + nameChannel + " :Cannot join channel (+l)");
		return;
	}

	// Todo OK -> unir al canal (modifica la estructura Channel)
	channel.addClient(clientFd);

	// Si venía por invitación, consumirla (la invitación se gasta)
	if (channel.isClientInvited(clientFd))
		channel.removeInvitedClient(clientFd);

	// A partir de aquí nos limitamos a delegar la construcción y envío de replies/broadcast
	// a la función auxiliar, para mantener esta función centrada en parsing/validación.
	// La función auxiliar asumirá que clientFd ya ha sido añadido a channel.clients.
	sendJoinInfo(clientFd, nameChannel);
}

// Envía a todos los miembros del canal los mensajes de unión, modos, topic y lista de usuarios
void Server::sendJoinInfo(int clientFd, const std::string &nameChannel)
{
	// Asumimos que nameChannel existe en _channels y que clientFd ya está en channel.clients
	Channel &channel = _channels[nameChannel];

	// Construcción del prefijo IRC correcto (nick!user@localhost)
	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;

	if (nick.empty())
		nick = convertIntToString(clientFd);

	if (user.empty())
		user = "user";

	std::string prefix = nick + "!" + user + "@localhost";

	// mensaje JOIN que verán todos los clientes del canal (broadcast)
	// Este broadcast incluye al propio cliente que acaba de unirse (como espera IRC).
	std::string joinmsg = ":" + prefix + " JOIN " + nameChannel;

	// Recorremos la lista de clientes del canal y enviamos el JOIN a cada uno
	std::set<int>::iterator iteratorMessageJoin = channel.clients.begin();
	while (iteratorMessageJoin != channel.clients.end())
	{
		int fd = *iteratorMessageJoin;

		// Si por algún motivo el fd no existe en nuestra tabla de clientes, lo saltamos.
		if (_clients.find(fd) == _clients.end())
		{
			++iteratorMessageJoin;
			continue;
		}

		sendNumericMessage(fd, joinmsg);
		++iteratorMessageJoin;
	}
	// Enviar estado de modos del canal (RPL_CHANNELMODEIS 324)
	// Construimos la cadena de modos y sus parámetros (si corresponde).
	std::string modes = "";
	std::string params = "";
	if (channel.isInviteOnly())
		modes = modes + "i";
	if (channel.isTopicRestricted())
		modes = modes + "t";
	if (channel.hasKey())
	{
		modes = modes + "k";
		params = params + " " + channel.getKey();
	}
	if (channel.getUserLimit() > 0)
	{
		modes = modes + "l";
		params = params + " " + convertIntToString(channel.getUserLimit());
	}

	if (!modes.empty())
	{
		std::string reply = ":ircserv 324 " + nick + " " + nameChannel + " +" + modes + params;
		sendNumericMessage(clientFd, reply);
	}
	// Enviar TOPIC (332) o NOTOPIC (331)
	// Muchos clientes esperan este reply antes de procesar NAMES.
	std::string topic = "";
	// asumimos que Channel tiene getTopic() que devuelve "" si no hay topic
	// si no existe, sustituye por la forma correcta de obtener el topic.
	topic = channel.getTopic();
	if (!topic.empty())
		sendNumericMessage(clientFd, ":ircserv 332 " + nick + " " + nameChannel + " :" + topic); // RPL_TOPIC 332 <nick> <channel> :<topic>
	else
		sendNumericMessage(clientFd, ":ircserv 331 " + nick + " " + nameChannel + " :No topic is set"); // RPL_NOTOPIC 331 <nick> <channel> :No topic is set

	// Enviar lista de usuarios del canal al cliente que entra (NAMES - 353)
	// Construimos la línea completa de NAMES y la enviamos en un solo sendNumericMessage
	// (si lo prefieres puedes enviar en múltiples paquetes, pero aquí respetamos
	// la forma en que lo tenías originalmente).
	std::string names = ":ircserv 353 " + nick + " = " + nameChannel + " :";

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
			entryNick = convertIntToString(fd);
		else
			entryNick = _clients[fd].nickname;

		if (channel.isClientOperator(fd))
			names = names + "@" + entryNick + " ";
		else
			names = names + entryNick + " ";

		++iteratorCreateList;
	}

	// Enviamos la lista y el marcador de fin de lista.
	sendNumericMessage(clientFd, names);
	sendNumericMessage(clientFd, ":ircserv 366 " + nick + " " + nameChannel + " :End of /NAMES list");

	// Mensaje por consola para depuración / logs
	std::cout << "JOIN: cliente fd " << clientFd << " unido a " << nameChannel << "\n";
}

// Permite a un cliente salir de un canal, notificando a todos los miembros y eliminando el canal si queda vacío
void Server::handlePartCommand(int clientFd, const std::string &line)
{
	// Prefijo "PART " longitud 5
	const size_t prefix_len = 5;
	if (line.size() <= prefix_len)
	{
		sendNumericMessage(clientFd, "461 PART :Not enough parameters");
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
		sendNumericMessage(clientFd, "461 PART :Not enough parameters");
		return;
	}
	// Comprueba si existe el canal
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		sendNumericMessage(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;
	// Comprueba que el emisor esté en el canal
	if (!channel.isClientInChannel(clientFd))
	{
		sendNumericMessage(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}
	// Construir prefijo: nick!user@localhost
	std::string emNick = _clients[clientFd].nickname;
	std::string emUser = _clients[clientFd].username;
	if (emNick.empty())
		emNick = convertIntToString(clientFd);
	if (emUser.empty())
		emUser = "user";
	std::string prefix = emNick + "!" + emUser + "@localhost";
	// Construir mensaje PART
	std::string out = ":" + prefix + " PART " + channelName;
	if (!reason.empty())
		out = out + " :" + reason;
	else
		out = out + " :";
	// Enviar PART a todos los miembros del canal (incluyendo el que sale)
	std::set<int>::iterator sit = channel.clients.begin();
	while (sit != channel.clients.end())
	{
		int fd = *sit;
		if (_clients.find(fd) != _clients.end())
			sendNumericMessage(fd, out);
		++sit;
	}
	// Quitar al cliente del canal (Channel::removeClient también quita operadores)
	channel.removeClient(clientFd);

	// Si el canal queda vacío, eliminarlo del mapa
	if (channel.clients.empty())
		_channels.erase(it);
}

// Procesa un mensaje privado o de canal (PRIVMSG)
// Analiza un mensaje PRIVMSG recibido, extrae el destinatario y el texto, construye el prefijo del emisor y luego delega el envío y procesamiento real del mensaje
void Server::handlePrivmsgCommand(int clientFd, const std::string &line)
{
	// "PRIVMSG <target> :<message>"
	const size_t prefix_len = 8; // strlen("PRIVMSG ")
	if (line.size() <= prefix_len)
	{
		sendNumericMessage(clientFd, "461 PRIVMSG :Not enough parameters");
		return;
	}

	// localizar espacio tras "PRIVMSG "
	size_t space = line.find(' ', prefix_len);
	if (space == std::string::npos)
	{
		sendNumericMessage(clientFd, "461 PRIVMSG :Not enough parameters");
		return;
	}
	// extraer target (nick o canal)
	std::string target = line.substr(prefix_len, space - prefix_len);

	// trim target por seguridad (inicio / fin)
	while (!target.empty() && (target[0] == ' ' || target[0] == '\t'))
		target.erase(0, 1);
	while (!target.empty() && (target[target.size() - 1] == ' ' || target[target.size() - 1] == '\t'))
		target.erase(target.size() - 1, 1);
	// localizar ':' que inicia el texto del mensaje
	size_t colon = line.find(':', space + 1);
	std::string text;

	if (colon == std::string::npos)
	{
		// No hay ':', pero puede que el cliente haya enviado el mensaje sin el prefijo ':'
		// Ej: "PRIVMSG nick hello there" -> tomar el resto tras el target como texto.
		text = line.substr(space + 1);
		// trim simple de inicio/fin
		while (!text.empty() && (text[0] == ' ' || text[0] == '\t'))
			text.erase(0, 1);
		while (!text.empty() && (text[text.size() - 1] == '\r' || text[text.size() - 1] == '\n'))
			text.erase(text.size() - 1, 1);

		if (text.empty())
		{
			sendNumericMessage(clientFd, "412 :No text to send");
			return;
		}
	}
	else
	{
		text = line.substr(colon + 1);
		if (text.empty())
		{
			sendNumericMessage(clientFd, "412 :No text to send");
			return;
		}
		// quitar posible CR al final
		if (!text.empty() && text[text.size() - 1] == '\r')
			text.erase(text.size() - 1);
	}
	// Construcción del prefijo IRC del emisor (nick!user@host)
	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;

	if (nick.empty())
		nick = convertIntToString(clientFd);

	if (user.empty())
		user = "user";

	std::string prefix = nick + "!" + user + "@localhost";

	processPrivmsgCommand(clientFd, target, text, prefix, nick);
}

// Procesa un mensaje PRIVMSG enviándolo al destinatario (usuario o canal), maneja respuestas automáticas 
// del bot y controla casos especiales como mensajes CTCP o transferencias DCC.
void Server::processPrivmsgCommand(int clientFd, const std::string &target, const std::string &text, const std::string &prefix, const std::string &nick)
{
	// INVOCAR AL BOT (solo si no es CTCP: no procesar mensajes que empiecen por \001)
	std::string botReply;
	bool isCTCP = (!text.empty() && text[0] == '\001');

	if (!isCTCP)
		botReply = _bot.generateReply(text, nick); // pasamos el texto tal cual al bot; el bot rellenará kickTarget si corresponde

	// SI EL MENSAJE ES DIRECTAMENTE AL BOT (PRIVADO)
	if (target == _bot.getName())
	{
		if (!botReply.empty() && !isCTCP)
		{
			std::string botOut =
				":" + _bot.getName() + "!bot@localhost PRIVMSG " +
				nick + " :" + botReply;

			sendNumericMessage(clientFd, botOut);
		}
		return;
	}
	// Si target es canal: reenviar como antes (sin DCC)
	if (!target.empty() && (target[0] == '#' || target[0] == '&' || target[0] == '+' || target[0] == '!'))
	{
		std::map<std::string, Channel>::iterator channelIterator = _channels.find(target);
		if (channelIterator == _channels.end())
		{
			sendNumericMessage(clientFd, "403 " + target + " :No such channel");
			return;
		}

		Channel &channel = channelIterator->second;

		if (!channel.isClientInChannel(clientFd))
		{
			sendNumericMessage(clientFd, "442 " + target + " :You're not on that channel");
			return;
		}

		// 1) Si el bot quiere responder al canal, enviamos su PRIVMSG desde el nick del bot
		if (!botReply.empty())
		{
			std::string botOut = ":" + _bot.getName() + "!bot@localhost PRIVMSG " + target + " :" + botReply;
			// Enviamos el mensaje del bot a todos los clientes del canal
			std::set<int>::iterator itb = channel.clients.begin();
			while (itb != channel.clients.end())
			{
				int fd = *itb;
				if (_clients.find(fd) != _clients.end())
					sendNumericMessage(fd, botOut);
				++itb;
			}
		}

		// 2) Reenviar el PRIVMSG original a todos los miembros del canal (igual que antes)
		std::string out = ":" + prefix + " PRIVMSG " + target + " :" + text;

		std::set<int>::iterator it = channel.clients.begin();
		while (it != channel.clients.end())
		{
			int fd = *it;
			if (fd == clientFd)
			{
				++it;
				continue;
			}
			if (_clients.find(fd) == _clients.end())
			{
				++it;
				continue;
			}
			sendNumericMessage(fd, out);
			++it;
		}
		return;
	}

	// Si target es usuario -> comprobar DCC (CTCP dentro del PRIVMSG)
	// Nota: /dcc send del cliente se traduce en un PRIVMSG con CTCP: PRIVMSG nick :\001DCC SEND <file> <ip> <port> <size>\001
	int dst_fd = findFdByNick(target);
	if (dst_fd == -1)
	{
		sendNumericMessage(clientFd, "401 " + target + " :No such nick");
		return;
	}

	// Si el texto comienza con 0x01 (CTCP) y contiene "DCC SEND " en la posición 1,
	// lo consideramos una petición DCC SEND que podemos interceptar.
	if (!text.empty() && text[0] == '\001')
	{
		const std::string dccPrefix = "DCC SEND ";
		size_t pos = text.find(dccPrefix, 1); // buscar a partir de la posición 1 (tras \001)
		if (pos == 1)
		{
			// Delegate toda la lógica DCC / FileTransfer a la función auxiliar.
			// La función devuelve true si se interceptó y manejó (incluye todos los fallbacks/NOTICEs)
			if (handleDccSend(clientFd, dst_fd, text, prefix, target))
				return; // ya manejado (interceptado o fallback)
		}
	}

	// Si no era un DCC SEND o el parse falló -> reenviar PRIVMSG clásico al destinatario
	// Pero antes, si el bot respondió (y no era CTCP), enviamos la respuesta del bot al destinatario
	if (!botReply.empty() && !isCTCP)
	{
		std::string botOut = ":" + _bot.getName() + "!bot@localhost PRIVMSG " + target + " :" + botReply;
		sendNumericMessage(dst_fd, botOut);
	}

	std::string out = ":" + prefix + " PRIVMSG " + target + " :" + text;
	sendNumericMessage(dst_fd, out);
}
