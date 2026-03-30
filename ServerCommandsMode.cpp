/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCommandsMode.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/10 14:47:22 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/30 11:07:02 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

// "MODE <#channel> +i" -> invite-only ON
// "MODE <#channel> -i" -> invite-only OFF
// nc: MODE <#channel> +i
// nc: MODE <#channel> -i
// irssi: /mode <#channel> +i
// irssi: /mode <#channel> -i
void Server::mode_i(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &prefix)
{
	if (!channel.isClientOperator(clientFd))
	{
		sendNumericMessage(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	channel.setInviteOnly(plus);

	std::string out = ":" + prefix + " MODE " + channelName + " ";
	if (plus)
		out = out + "+i";
	else
		out = out + "-i";

	std::set<int>::iterator iterator = channel.clients.begin();
	while (iterator != channel.clients.end())
	{
		int fd = *iterator;
		if (_clients.find(fd) != _clients.end())
			sendNumericMessage(fd, out);
		++iterator;
	}
}

// "MODE <#channel> +t" -> only operators can change topic
// "MODE <#channel> -t" -> NO only operators can change topic
// nc: MODE <#channel> +t
// nc: MODE <#channel> -t
// irssi: /mode <#channel> +t
// irssi: /mode <#channel> -t
void Server::mode_t(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &prefix)
{
	if (!channel.isClientOperator(clientFd))
	{
		sendNumericMessage(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	channel.setTopicRestricted(plus);

	std::string out = ":" + prefix + " MODE " + channelName + " ";
	if (plus)
		out = out + "+t";
	else
		out = out + "-t";

	std::set<int>::iterator iterator = channel.clients.begin();
	while (iterator != channel.clients.end())
	{
		int fd = *iterator;
		if (_clients.find(fd) != _clients.end())
			sendNumericMessage(fd, out);
		++iterator;
	}
}

// "MODE <#channel> +k <password>"  -> set password to join channel
// "MODE <#channel> -k" -> remove password to join channel
// nc: MODE <#channel> +k <password>
// nc: MODE <#channel> -k
// irssi: /mode <#channel> +k <password>
// irssi: /mode <#channel> -k
void Server::mode_k(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &param, const std::string &prefix)
{
	if (!channel.isClientOperator(clientFd))
	{
		sendNumericMessage(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	if (plus)
	{
		channel.setKey(param);
		std::string out = ":" + prefix + " MODE " + channelName + " +k " + param;

		std::set<int>::iterator iterator = channel.clients.begin();
		while (iterator != channel.clients.end())
		{
			int fd = *iterator;
			if (_clients.find(fd) != _clients.end())
				sendNumericMessage(fd, out);
			++iterator;
		}
	}
	else
	{
		channel.removeKey();
		std::string out = ":" + prefix + " MODE " + channelName + " -k";

		std::set<int>::iterator iterator = channel.clients.begin();
		while (iterator != channel.clients.end())
		{
			int fd = *iterator;
			if (_clients.find(fd) != _clients.end())
				sendNumericMessage(fd, out);
			++iterator;
		}
	}
}

// "MODE <#channel> +o <nick>" -> assign an operator to a user
// "MODE <#channel> -o <nick>" -> remove operator from a user
// nc: MODE <#channel> +o <nick>
// nc: MODE <#channel> -o <nick>
// irssi: /mode <#channel> +o <nick>
// irssi: /mode <#channel> -o <nick>
void Server::mode_o(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &targetNick, const std::string &prefix)
{
	if (!channel.isClientOperator(clientFd))
	{
		sendNumericMessage(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	int targetFd = findFdByNick(targetNick);
	if (targetFd == -1)
	{
		sendNumericMessage(clientFd, "401 " + targetNick + " :No such nick");
		return;
	}

	if (!channel.isClientInChannel(targetFd))
	{
		sendNumericMessage(clientFd, "441 " + targetNick + " " + channelName + " :They aren't on that channel");
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
			sendNumericMessage(fd, out);
		++iterator;
	}
}

// "MODE <#channel> +l <numberClients>" -> limit users per channel
// "MODE <#channel> -l" -> remove limit users per channel
// nc: MODE <#channel> +l <numberClients>
// nc: MODE <#channel> -l
// irssi: /mode <#channel> +l <numberClients>
// irssi: /mode <#channel> -l
void Server::mode_l(int clientFd, Channel &channel, const std::string &channelName, bool plus, const std::string &param, const std::string &prefix)
{
	if (!channel.isClientOperator(clientFd))
	{
		sendNumericMessage(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	if (plus)
	{
		const char *s = param.c_str();
		char *endptr = NULL;
		long v = ft_strtol(s, &endptr);
		if (*endptr != '\0' || v < 0)
		{
			sendNumericMessage(clientFd, "461 MODE :Not enough parameters");
			return;
		}
		channel.setUserLimit((int)v);
		std::string out = ":" + prefix + " MODE " + channelName + " +l " + convertIntToString((int)v);

		std::set<int>::iterator iterator = channel.clients.begin();
		while (iterator != channel.clients.end())
		{
			int fd = *iterator;
			if (_clients.find(fd) != _clients.end())
				sendNumericMessage(fd, out);
			++iterator;
		}
	}
	else
	{
		channel.removeUserLimit();
		std::string out = ":" + prefix + " MODE " + channelName + " -l";

		std::set<int>::iterator iterator = channel.clients.begin();
		while (iterator != channel.clients.end())
		{
			int fd = *iterator;
			if (_clients.find(fd) != _clients.end())
				sendNumericMessage(fd, out);
			++iterator;
		}
	}
}

// "MODE <nick> +i" -> user invisible
// "MODE <nick> -i" -> ruser no invisible
// nc: MODE <nick> +i
// nc: MODE <nick> -i
// irssi: /mode <nick> +i
// irssi: /mode <nick> -i
void Server::mode_user(int clientFd, const std::string &target, const std::string &rest)
{
	std::map<int, Client>::iterator itClient = _clients.find(clientFd);
	if (itClient == _clients.end())
		return;

	std::string myNick = itClient->second.nickname;
	if (myNick.empty() || myNick != target)
	{
		sendNumericMessage(clientFd, "502 :Cannot change mode for other users"); // ERR_USERSDONTMATCH 502
		return;
	}
	
	std::string modes = rest;

	while (!modes.empty() && (modes[0] == ' ' || modes[0] == '\t'))
		modes.erase(0, 1);

	size_t space = modes.find(' ');
	if (space != std::string::npos)
		modes = modes.substr(0, space);

	if (!modes.empty() && modes[modes.size() - 1] == '\r')
		modes.erase(modes.size() - 1);

	if (modes.empty())
	{
		std::string reply = " :ircserv 221 " + myNick + " :User modes";
		sendNumericMessage(clientFd, reply);
		return;
	}

	bool plus = true;
	std::string applied; 
	size_t i = 0;
	while (i < modes.size())
	{
		char c = modes[i];
		i++;

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
				std::cout << "fd " << clientFd << " User " << myNick << " is now invisible\n";
			else
				std::cout << "fd " << clientFd << " User " << myNick << " is no longer invisible\n";

			if (plus)
				applied = applied + "+i";
			else
				applied = applied + "-i";
		}
		else
			sendNumericMessage(clientFd, "501 :Unknown MODE flag"); // UMODEUNKNOWNFLAG 501
	}

	if (!applied.empty())
	{
		std::string echo = ":" + myNick + " MODE " + target + " " + applied;
		sendNumericMessage(clientFd, echo);
	}
}
