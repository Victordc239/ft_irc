/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCommands.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 16:07:46 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/30 13:58:03 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

// CHECK the password and ignore any CAP
bool Server::handleAuthenticationCmds(size_t &i, const std::string &line)
{
	int clientFd = _fds[i].fd;

	// "PASS <password>"
	// nc: PASS <password>
	// irssi: automatic
	if (line.compare(0, 5, "PASS ") == 0) // Check the password
	{
		std::string given = line.substr(5);

		if (given == _serverPassword)
		{
			_clients[clientFd].correctPass = true;
			std::cout << "fd " << clientFd << " correct PASS\n";
		}
		else
		{
			sendNumericMessage(clientFd, "464 :Password incorrect");
			std::cout << "fd " << clientFd << " incorrect PASS\n";
		}
	}
	// "CAP LS / CAP END / CAP *"
	// nc: CAP LS
	// irssi: automatic
	else if (line.compare(0, 4, "CAP ") == 0) // We ignore any CAP
	{
		std::string rest = line.substr(4);
		size_t start = 0;
		while (start < rest.size() && (rest[start] == ' ' || rest[start] == '\t'))
			start++;

		rest = rest.substr(start);
		size_t space = rest.find(' ');
		std::string subcmd;

		if (space == std::string::npos)
			subcmd = rest;
		else
			subcmd = rest.substr(0, space);
		if (!subcmd.empty() && subcmd[subcmd.size() - 1] == '\r')
			subcmd.erase(subcmd.size() - 1);

		if (subcmd == "LS")
		{
			sendNumericMessage(clientFd, "CAP * LS :");
			std::cout << "fd " << clientFd << " CAP LS received (ignored)\n";
		}
		else if (subcmd == "END")
			std::cout << "fd " << clientFd << " CAP END received (ignored)\n";
		else
			std::cout << "fd " << clientFd << " CAP " << subcmd << " received (ignored)\n"; 
	}
	return (false);
}

// SET a nick to a new client
// "NICK <nickname>"
// nc: NICK <nickname>
// irssi: /nick <nickname>
void Server::handleNickCommand(int clientFd, const std::string &line)
{
	std::string newnick;
	if (line.size() > 5)
		newnick = line.substr(5);

	size_t start = 0;
	while (start < newnick.size() && (newnick[start] == ' ' || newnick[start] == '\t'))
		++start;
	size_t end = newnick.size();
	while (end > start && (newnick[end - 1] == ' ' || newnick[end - 1] == '\t'))
		--end;
	newnick = newnick.substr(start, end - start);

	size_t space = newnick.find(' ');
	if (space != std::string::npos)
		newnick = newnick.substr(0, space);
	if (!newnick.empty() && newnick[newnick.size() - 1] == '\r')
		newnick.erase(newnick.size() - 1);
	if (newnick.empty())
	{
		sendNumericMessage(clientFd, "431 :No nickname given"); // ERR_NONICKNAMEGIVEN 431
		return;
	}
	if (isNickInUse(newnick))
	{
		std::string current = _clients[clientFd].nickname;
		if (current.empty())
			current = "*";
		std::string err = ":ircserv 433 " + current + " " + newnick + " :Nickname is already in use"; // ERR_NICKNAMEINUSE 433
		sendNumericMessage(clientFd, err);
		return;
	}
	_clients[clientFd].nickname = newnick;
	std::cout << "fd " << clientFd << " set NICK=" << newnick << "\n";
}

// SET a user to a new client
// "USER <username>"
// nc: USER <username> 
// irssi: automatic
void Server::handleUserCommand(int clientFd, const std::string &line)
{
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
		sendNumericMessage(clientFd, "461 USER :Not enough parameters"); 	// ERR_NEEDMOREPARAMS 461
}

// JOIN a channel
// "JOIN <#channel> [key]"
// nc: JOIN <#channel>
// nc: JOIN <#channel> [key]
// irssi: /join <#channel>
// irssi: /join <#channel> [key]
void Server::handleJoinCommand(int clientFd, const std::string &line)
{
	std::string rest;
	if (line.size() > 5)
		rest = line.substr(5);
	while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t'))
		rest.erase(0, 1);
	while (!rest.empty() && (rest[rest.size() - 1] == ' ' || rest[rest.size() - 1] == '\t'))
		rest.erase(rest.size() - 1, 1);
	if (rest.empty())
	{
		sendNumericMessage(clientFd, "461 JOIN :Not enough parameters");
		return;
	}
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
		while (!joinKey.empty() && (joinKey[0] == ' ' || joinKey[0] == '\t'))
			joinKey.erase(0, 1);
		while (!joinKey.empty() && (joinKey[joinKey.size() - 1] == ' ' || joinKey[joinKey.size() - 1] == '\t'))
			joinKey.erase(joinKey.size() - 1, 1);
	}
	if (!nameChannel.empty() && nameChannel[nameChannel.size() - 1] == '\r')
		nameChannel.erase(nameChannel.size() - 1, 1);
	if (!joinKey.empty() && joinKey[joinKey.size() - 1] == '\r')
		joinKey.erase(joinKey.size() - 1, 1);
	if (nameChannel.empty())
	{
		sendNumericMessage(clientFd, "461 JOIN :Not enough parameters");
		return;
	}
	if (nameChannel[0] != '#' && nameChannel[0] != '&' && nameChannel[0] != '+' && nameChannel[0] != '!')
	{
		sendNumericMessage(clientFd, "403 " + nameChannel + " :Invalid channel prefix");
		return;
	}

	if (_channels.find(nameChannel) == _channels.end()) //Create a new channel and the client is the operator
	{
		Channel newChannel(nameChannel);
		newChannel.addOperator(clientFd);
		_channels[nameChannel] = newChannel;
	}

	Channel &channel = _channels[nameChannel];
	if (channel.isClientInChannel(clientFd))
	{
		std::cout << "JOIN: fd " << clientFd << " was already in " << nameChannel << "\n";
		return;
	}
	
	if (channel.isInviteOnly() && !channel.isClientInvited(clientFd)) // CHECK: invite-only (+i)
	{
		
		std::string nick = _clients[clientFd].nickname;
		if (nick.empty())
			nick = convertIntToString(clientFd);
		sendNumericMessage(clientFd, "473 " + nick + " " + nameChannel + " :Cannot join channel (+i)"); // ERR_INVITEONLYCHAN 473
		return;
	}

	if (channel.hasKey()) // CHECK: key (+k)
	{
		if (joinKey.empty() || joinKey != channel.getKey())
		{
			std::string nick = _clients[clientFd].nickname;
			if (nick.empty())
				nick = convertIntToString(clientFd);
			sendNumericMessage(clientFd, "475 " + nick + " " + nameChannel + " :Cannot join channel (+k)"); // ERR_BADCHANNELKEY 475
			return;
		}
	}

	if (channel.getUserLimit() > 0 && (int)channel.clients.size() >= channel.getUserLimit()) // CHECK: limit (+l)
	{
		std::string nick = _clients[clientFd].nickname;
		if (nick.empty())
			nick = convertIntToString(clientFd);
		sendNumericMessage(clientFd, "471 " + nick + " " + nameChannel + " :Cannot join channel (+l)");
		return;
	}

	channel.addClient(clientFd);
	if (channel.isClientInvited(clientFd))
		channel.removeInvitedClient(clientFd);

	sendJoinInfo(clientFd, nameChannel);
}

// Send all channel members the join messages, modes, topic, and user list
void Server::sendJoinInfo(int clientFd, const std::string &nameChannel)
{
	Channel &channel = _channels[nameChannel];
	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;
	if (nick.empty())
		nick = convertIntToString(clientFd);
	if (user.empty())
		user = "user";

	std::string prefix = nick + "!" + user + "@localhost";
	std::string joinmsg = ":" + prefix + " JOIN " + nameChannel;
	std::set<int>::iterator iteratorMessageJoin = channel.clients.begin();
	while (iteratorMessageJoin != channel.clients.end())
	{
		int fd = *iteratorMessageJoin;
		if (_clients.find(fd) == _clients.end())
		{
			++iteratorMessageJoin;
			continue;
		}
		sendNumericMessage(fd, joinmsg);
		++iteratorMessageJoin;
	}
	// Send channel mode status (RPL_CHANNELMODEIS 324)
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
	// send topic
	std::string topic = "";
	topic = channel.getTopic();
	if (!topic.empty())
		sendNumericMessage(clientFd, ":ircserv 332 " + nick + " " + nameChannel + " :" + topic); // RPL_TOPIC 332
	else
		sendNumericMessage(clientFd, ":ircserv 331 " + nick + " " + nameChannel + " :No topic is set"); // RPL_NOTOPIC 331

	// Send user list
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
	sendNumericMessage(clientFd, names);
	sendNumericMessage(clientFd, ":ircserv 366 " + nick + " " + nameChannel + " :End of /NAMES list");

	std::cout << "JOIN: client fd " << clientFd << " join a " << nameChannel << "\n";
}

// Client leaves a channel, it notifies all members and deletes the channel if it becomes empty.
// "PART <#channel> [:<reason>]"
// nc: PART <#channel> 
// nc: PART <#channel> [:<reason>]
// irssi: /part <#channel> 
// irssi: /part <#channel> [:<reason>]
void Server::handlePartCommand(int clientFd, const std::string &line)
{
	const size_t prefix_len = 5;
	if (line.size() <= prefix_len)
	{
		sendNumericMessage(clientFd, "461 PART :Not enough parameters");
		return;
	}
	size_t space = line.find(' ', prefix_len);
	std::string channelName;
	std::string reason;

	if (space == std::string::npos)
	{
		channelName = line.substr(prefix_len);
		if (!channelName.empty() && channelName[channelName.size() - 1] == '\r')
			channelName.erase(channelName.size() - 1, 1);
	}
	else //find reason; if there's no reason, ignore it.
	{
		channelName = line.substr(prefix_len, space - prefix_len);
		size_t colon = line.find(':', space + 1);
		if (colon != std::string::npos)
		{
			reason = line.substr(colon + 1);
			if (!reason.empty() && reason[reason.size() - 1] == '\r')
				reason.erase(reason.size() - 1, 1);
		}
		else
		{
			std::string maybe = line.substr(space + 1);
			(void)maybe;
		}
	}
	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);
	if (channelName.empty())
	{
		sendNumericMessage(clientFd, "461 PART :Not enough parameters");
		return;
	}
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		sendNumericMessage(clientFd, "403 " + channelName + " :No such channel");
		return;
	}
	Channel &channel = it->second;
	if (!channel.isClientInChannel(clientFd))
	{
		sendNumericMessage(clientFd, "442 " + channelName + " :You're not on that channel");
		return;
	}
	std::string emNick = _clients[clientFd].nickname;
	std::string emUser = _clients[clientFd].username;
	if (emNick.empty())
		emNick = convertIntToString(clientFd);
	if (emUser.empty())
		emUser = "user";
	std::string prefix = emNick + "!" + emUser + "@localhost";
	std::string out = ":" + prefix + " PART " + channelName;
	if (!reason.empty())
		out = out + " :" + reason;
	else
		out = out + " :";
	std::set<int>::iterator sit = channel.clients.begin();
	while (sit != channel.clients.end())
	{
		int fd = *sit;
		if (_clients.find(fd) != _clients.end())
			sendNumericMessage(fd, out);
		++sit;
	}
	channel.removeClient(clientFd);

	// Deletes the channel if it becomes empty
	if (channel.clients.empty())
		_channels.erase(it);
}

// Handle a private message
// "PRIVMSG <target> :<message>
// nc:    PRIVMSG <targetCHANNEL> :<message>
// nc:    PRIVMSG <targetUSER> :<message>
// irssi: /msg <targetCHANNEL> :<message>
// irssi: /msg <target> :<message>
void Server::handlePrivmsgCommand(int clientFd, const std::string &line)
{
	const size_t prefix_len = 8;
	if (line.size() <= prefix_len)
	{
		sendNumericMessage(clientFd, "461 PRIVMSG :Not enough parameters");
		return;
	}
	size_t space = line.find(' ', prefix_len);
	if (space == std::string::npos)
	{
		sendNumericMessage(clientFd, "461 PRIVMSG :Not enough parameters");
		return;
	}
	std::string target = line.substr(prefix_len, space - prefix_len);
	while (!target.empty() && (target[0] == ' ' || target[0] == '\t'))
		target.erase(0, 1);
	while (!target.empty() && (target[target.size() - 1] == ' ' || target[target.size() - 1] == '\t'))
		target.erase(target.size() - 1, 1);
	size_t colon = line.find(':', space + 1);
	std::string text;

	if (colon == std::string::npos)
	{
		text = line.substr(space + 1);
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
		if (!text.empty() && text[text.size() - 1] == '\r')
			text.erase(text.size() - 1);
	}
	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;
	if (nick.empty())
		nick = convertIntToString(clientFd);
	if (user.empty())
		user = "user";
	std::string prefix = nick + "!" + user + "@localhost";

	processPrivmsgCommand(clientFd, target, text, prefix, nick);
}

void Server::processPrivmsgCommand(int clientFd, const std::string &target, const std::string &text, const std::string &prefix, const std::string &nick)
{
	std::string botReply;
	bool isCTCP = (!text.empty() && text[0] == '\001');

	if (!isCTCP)
		botReply = _bot.generateReply(text, nick);

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

		if (!botReply.empty())
		{
			std::string botOut = ":" + _bot.getName() + "!bot@localhost PRIVMSG " + target + " :" + botReply;
			std::set<int>::iterator itb = channel.clients.begin();
			while (itb != channel.clients.end())
			{
				int fd = *itb;
				if (_clients.find(fd) != _clients.end())
					sendNumericMessage(fd, botOut);
				++itb;
			}
		}

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

	// DCC SEND <nickTarget> <file_path>
	int dst_fd = findFdByNick(target);
	if (dst_fd == -1)
	{
		sendNumericMessage(clientFd, "401 " + target + " :No such nick");
		return;
	}

	if (!text.empty() && text[0] == '\001')
	{
		const std::string dccPrefix = "DCC SEND ";
		size_t pos = text.find(dccPrefix, 1);
		if (pos == 1)
		{
			if (handleDccSend(clientFd, dst_fd, text, prefix, target))
				return;
		}
	}

	if (!botReply.empty() && !isCTCP)
	{
		std::string botOut = ":" + _bot.getName() + "!bot@localhost PRIVMSG " + target + " :" + botReply;
		sendNumericMessage(dst_fd, botOut);
	}

	std::string out = ":" + prefix + " PRIVMSG " + target + " :" + text;
	sendNumericMessage(dst_fd, out);
}
