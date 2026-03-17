/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCommands.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 16:07:46 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/17 14:47:32 by sofernan         ###   ########.fr       */
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

bool Server::handleDccSendInPrivmsg(int clientFd, int dst_fd, const std::string &text, const std::string &prefix, const std::string &target)
{
	/* ===========================================================
		Si el texto comienza con 0x01 (CTCP) y contiene "DCC SEND " en la posición 1,
		lo consideramos una petición DCC SEND que podemos interceptar.
		(esta función asume que ya comprobaste que text[0] == '\001' y que
		 text.find("DCC SEND ",1) == 1)
		=========================================================== */

	const std::string dccPrefix = "DCC SEND ";
	size_t pos = 1; // precondición de llamada

	// Extraer la porción después de "DCC SEND "
	size_t start = pos + dccPrefix.size(); // inicio de los tokens: filename ip port size
	std::string rest = "";
	if (start < text.size())
		rest = text.substr(start);

	// quitar un posible \001 final
	if (!rest.empty() && rest[rest.size() - 1] == '\001')
		rest.erase(rest.size() - 1);

	// Tokenizar rest por espacios de forma manual:
	std::vector<std::string> toks;
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
			toks.push_back(s);
			break;
		}
		toks.push_back(s.substr(0, p));
		s.erase(0, p + 1);
	}

	// Esperamos al menos el filename (toks[0]). ip/port/size son opcionales
	if (!toks.empty())
	{
		std::string filename = toks[0];
		unsigned long fsize = 0;
		// si nos dan size (habitualmente en toks[3]) intentamos parsearlo
		if (toks.size() >= 4)
		{
			char *endptr = NULL;
			fsize = (unsigned long)ftStrtol(toks[3].c_str(), &endptr);
			if (*endptr != '\0')
				fsize = 0; // parse error -> 0
		}

		// Construir la transferencia y crear listener (proxy)
		FileTransfer ft;
		ft.id = _nextTransferId++;
		ft.senderFd = clientFd;
		ft.receiverFd = dst_fd;
		ft.filename = filename;
		ft.filesize = fsize;
		ft.bytesTransferred = 0;
		ft.senderClosed = false;
		ft.receiverClosed = false;
		ft.listenerCreated = false;
		ft.bothConnected = false;

		// Intentar crear el listener (ephemeral port)
		if (ft.createListener() != 0)
		{
			// fallo: avisar al emisor y reenviar el PRIVMSG original como fallback
			sendNumeric(clientFd, ":ircserv NOTICE :DCC proxy failed to create listener");
			std::string fallback = ":" + prefix + " PRIVMSG " + target + " :" + text;
			sendNumeric(dst_fd, fallback);
			return true;
		}

		// Guardar la transferencia en el mapa del servidor (copiamos metadata).
		_transfers[ft.id] = ft;

		// transferir ownership del fd al objeto en el mapa
		_transfers[ft.id].socketFileTransfer = ft.socketFileTransfer;
		// evitar que el destructor del objeto local cierre el fd (liberar propiedad)
		ft.socketFileTransfer = -1;

		// ahora sí obtenemos el fd real
		int lfd = _transfers[ft.id].socketFileTransfer;
		if (lfd == -1)
		{
			// fallback seguro (no debería ocurrir si createListener() tuvo éxito)
			sendNumeric(clientFd, ":ircserv NOTICE :DCC proxy internal error (no listener fd)");
			return true;
		}

		// Añadir listener al array de poll para que runLoop() lo gestione
		pollfd pfd;
		pfd.fd = lfd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_fds.push_back(pfd);

		// Mapear el fd del listener a la transferencia
		_fdToTransferId[lfd] = ft.id;

		/* -------------------------------------------------------------
			Intentar conectar de forma activa al sender ORIGINAL (si el
			CTCP incluía IP y PORT). Muchos clientes (el emisor) están en
			modo "listen" y esperan que el receptor conecte a ellos. Si
			queremos recibir bytes en el proxy, debemos conectarnos al
			sender usando la IP/PORT que nos mandó.
			------------------------------------------------------------- */

		// tratar de recuperar IP y PORT originales del sender si estaban en toks
		if (toks.size() >= 3)
		{
			std::string orig_ip_tok = toks[1];
			std::string orig_port_tok = toks[2];
			// convertir IP: puede venir como decimal (host order) o dotted; soportamos ambos
			struct sockaddr_in sender_addr;
			std::memset(&sender_addr, 0, sizeof(sender_addr));
			sender_addr.sin_family = AF_INET;
			bool haveSenderAddr = false;

			struct in_addr ina;
			if (parseDccIpToken(orig_ip_tok, ina))
			{
				sender_addr.sin_addr = ina;
				haveSenderAddr = true;
			}

			int sender_port = 0;
			if (haveSenderAddr)
			{
				char *endptr_port = NULL;
				sender_port = (int)strtol(orig_port_tok.c_str(), &endptr_port, 10);
				if (*endptr_port != '\0' || sender_port <= 0 || sender_port > 65535)
					haveSenderAddr = false;
				else
					sender_addr.sin_port = htons((uint16_t)sender_port);
			}

			if (haveSenderAddr)
			{
				// crear socket y conectarnos al sender (non-blocking)
				int s = socket(AF_INET, SOCK_STREAM, 0);
				if (s != -1)
				{
					// poner non-blocking
					int fl = fcntl(s, F_GETFL, 0);
					if (fl != -1)
						fcntl(s, F_SETFL, fl | O_NONBLOCK);

					int cres = connect(s, (struct sockaddr*)&sender_addr, sizeof(sender_addr));
					if (cres == 0)
					{
						// conectado de inmediato
						_transfers[ft.id].senderFdRedDDC = s;
						_transfers[ft.id].senderClosed = false;

						pollfd sp;
						sp.fd = s;
						sp.events = POLLIN;
						sp.revents = 0;
						_fds.push_back(sp);

						_fdToTransferId[s] = ft.id;
					}
					else if (errno == EINPROGRESS || errno == EINTR)
					{
						// conexión en progreso: vigilamos POLLOUT para saber cuándo termina
						_transfers[ft.id].senderFdRedDDC = s;
						_transfers[ft.id].senderClosed = false;

						pollfd sp;
						sp.fd = s;
						sp.events = POLLIN | POLLOUT;
						sp.revents = 0;
						_fds.push_back(sp);

						_fdToTransferId[s] = ft.id;

					}
					else
						close(s);
				}
				else
					sendNumeric(clientFd, ":ircserv NOTICE :DCC proxy could not create outbound socket to sender; proxy will wait for connections");
			}
			else
				sendNumeric(clientFd, ":ircserv NOTICE :DCC proxy couldn't parse sender address from CTCP; proxy will wait for incoming connections");
		}

		// Obtener IP del servidor para enviar al receptor (si no se puede, usar 127.0.0.1)
		// NOTA: no usar getsockname(_server_fd) cuando _server_fd está ligado a INADDR_ANY,
		// porque devolvería 0.0.0.0. En su lugar intentamos averiguar la IP saliente
		// creando un socket UDP y "conectándolo" a IP pública (no se envía tráfico).
		std::string serverIp = "127.0.0.1";
		{
			int sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock != -1)
			{
				struct sockaddr_in remote;
				std::memset(&remote, 0, sizeof(remote));
				remote.sin_family = AF_INET;
				// conectar a una IP pública solo para conocer la interfaz saliente
				remote.sin_addr.s_addr = inet_addr("8.8.8.8");
				remote.sin_port = htons(53);
				// connect no envía paquetes en UDP, solo deja elegir interfaz
				if (connect(sock, (struct sockaddr*)&remote, sizeof(remote)) != -1)
				{
					struct sockaddr_in name;
					socklen_t namelen = sizeof(name);
					if (getsockname(sock, (struct sockaddr*)&name, &namelen) != -1)
					{
						char ipbuf[INET_ADDRSTRLEN];
						if (inet_ntop(AF_INET, &name.sin_addr, ipbuf, sizeof(ipbuf)) != NULL)
							serverIp = ipbuf;
					}
				}
				close(sock);
			}
		}

		// Convertir IP (dotted) a entero decimal que usa DCC (ntohl(inet_addr(ip)))
		uint32_t ip_net = inet_addr(serverIp.c_str()); // network order
		uint32_t ip_decimal = ntohl(ip_net);          // host order decimal

		// Construir el CTCP DCC SEND modificado para el receptor (le decimos que se conecte al servidor)
		unsigned short port = _transfers[ft.id].getListenerPort();
		if (port == 0)
		{
			// fallback: puerto inválido
			sendNumeric(clientFd, ":ircserv NOTICE :DCC proxy internal error (listener port=0)");
			return true;
		}

		// Formato clásico DCC: IP en decimal (host order) y puerto en decimal.
		std::string dccmsg = "\001DCC SEND " + filename + " " + intToString((int)ip_decimal) + " " + intToString((int)port) + " " + intToString((int)fsize) + "\001";
		std::string outmsg = ":" + prefix + " PRIVMSG " + target + " :" + dccmsg;

		// Enviar al receptor el CTCP con IP/puerto del proxy
		sendNumeric(dst_fd, outmsg);

		// Informar al emisor (opcional, mensaje NOTICE)
		std::string senderNick = _clients[clientFd].nickname;
		if (senderNick.empty())
			senderNick = intToString(clientFd);
		sendNumeric(clientFd, ":ircserv NOTICE " + senderNick + " :DCC proxy created id=" + intToString((int)ft.id));

		// Fin: no reenviamos el PRIVMSG original (interceptado)
		return true;
	}

	// Si no había tokens (no filename) -> no hicimos nada especial
	return false;
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

	// localizar espacio tras "PRIVMSG "
	size_t space = line.find(' ', prefix_len);
	if (space == std::string::npos)
	{
		sendNumeric(clientFd, "461 PRIVMSG :Not enough parameters");
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
			sendNumeric(clientFd, "412 :No text to send");
			return;
		}
	}
	else
	{
		text = line.substr(colon + 1);
		if (text.empty())
		{
			sendNumeric(clientFd, "412 :No text to send");
			return;
		}
		// quitar posible CR al final
		if (!text.empty() && text[text.size() - 1] == '\r')
			text.erase(text.size() - 1);
	}

	/* ===========================================================
		Construcción del prefijo IRC del emisor (nick!user@host)
		=========================================================== */
	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;

	if (nick.empty())
		nick = intToString(clientFd);

	if (user.empty())
		user = "user";

	std::string prefix = nick + "!" + user + "@localhost";

	/* ===========================================================
		INVOCAR AL BOT (solo si no es CTCP: no procesar mensajes que empiecen por \001)
		=========================================================== */
	std::string botReply;
	bool isCTCP = (!text.empty() && text[0] == '\001');

	if (!isCTCP)
		botReply = _bot.generateReply(text, nick); // pasamos el texto tal cual al bot; el bot rellenará kickTarget si corresponde

	/* ===========================================================
		SI EL MENSAJE ES DIRECTAMENTE AL BOT (PRIVADO)
		=========================================================== */
	if (target == _bot.getName())
	{
		if (!botReply.empty() && !isCTCP)
		{
			std::string botOut =
				":" + _bot.getName() + "!bot@localhost PRIVMSG " +
				nick + " :" + botReply;

			sendNumeric(clientFd, botOut);
		}
		return;
	}

	/* ===========================================================
		Si target es canal: reenviar como antes (sin DCC)
		=========================================================== */
	if (!target.empty() && (target[0] == '#' || target[0] == '&' || target[0] == '+' || target[0] == '!'))
	{
		std::map<std::string, Channel>::iterator channelIterator = _channels.find(target);
		if (channelIterator == _channels.end())
		{
			sendNumeric(clientFd, "403 " + target + " :No such channel");
			return;
		}

		Channel &channel = channelIterator->second;

		if (!channel.hasClient(clientFd))
		{
			sendNumeric(clientFd, "442 " + target + " :You're not on that channel");
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
					sendNumeric(fd, botOut);
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
			sendNumeric(fd, out);
			++it;
		}
		return;
	}

	/* ===========================================================
		Si target es usuario -> comprobar DCC (CTCP dentro del PRIVMSG)
		Nota: /dcc send del cliente se traduce en un PRIVMSG con CTCP:
			PRIVMSG nick :\001DCC SEND <file> <ip> <port> <size>\001
		=========================================================== */

	int dst_fd = getFdByNick(target);
	if (dst_fd == -1)
	{
		sendNumeric(clientFd, "401 " + target + " :No such nick");
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
			if (handleDccSendInPrivmsg(clientFd, dst_fd, text, prefix, target))
				return; // ya manejado (interceptado o fallback)
		}
	}

	// Si no era un DCC SEND o el parse falló -> reenviar PRIVMSG clásico al destinatario
	// Pero antes, si el bot respondió (y no era CTCP), enviamos la respuesta del bot al destinatario
	if (!botReply.empty() && !isCTCP)
	{
		std::string botOut = ":" + _bot.getName() + "!bot@localhost PRIVMSG " + target + " :" + botReply;
		sendNumeric(dst_fd, botOut);
	}

	std::string out = ":" + prefix + " PRIVMSG " + target + " :" + text;
	sendNumeric(dst_fd, out);
}
