/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerOperatorsCommands.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/09 14:39:18 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/30 14:14:21 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

// "KICK <#channel> <nick> [:<reasonExpulsionChannel>]"
// nc: KICK <#channel> <nick>
// nc: KICK <#channel> <nick> [:<reasonExpulsionChannel>]
// irssi: /kick <#channel> <nick>
// irssi: /kick <#channel> <nick> [:<reasonExpulsionChannel>]
void	Server::handleKickCommand(int clientFd, const std::string &line)
{
	const size_t prefix_len = 5;
	if (line.size() <= prefix_len)
	{
		sendNumericMessage(clientFd, "461 KICK :Not enough parameters");
		return;
	}

	size_t space = line.find(' ', prefix_len);
	if (space == std::string::npos)
	{
		sendNumericMessage(clientFd, "461 KICK :Not enough parameters");
		return;
	}
	std::string channelName = line.substr(prefix_len, space - prefix_len);

	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

	std::string targetNick;
	std::string comment;
	size_t colon = line.find(':', space + 1);
	if (colon == std::string::npos)
	{
		targetNick = line.substr(space + 1);
		if (!targetNick.empty() && targetNick[targetNick.size() - 1] == '\r')
			targetNick.erase(targetNick.size() - 1, 1);
	}
	else
	{
		if (colon > space + 1)
			targetNick = line.substr(space + 1, colon - (space + 1));
		else
			targetNick = "";
		comment = line.substr(colon + 1);
		if (!comment.empty() && comment[comment.size() - 1] == '\r')
			comment.erase(comment.size() - 1, 1);
	}
	while (!targetNick.empty() && (targetNick[0] == ' ' || targetNick[0] == '\t'))
		targetNick.erase(0, 1);
	while (!targetNick.empty() && (targetNick[targetNick.size() - 1] == ' ' || targetNick[targetNick.size() - 1] == '\t'))
		targetNick.erase(targetNick.size() - 1, 1);

	if (targetNick.empty())
	{
		sendNumericMessage(clientFd, "461 KICK :Not enough parameters");
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
	std::string emNick = _clients[clientFd].nickname;
	std::string emUser = _clients[clientFd].username;
	if (emNick.empty())
		emNick = convertIntToString(clientFd);
	if (emUser.empty())
		emUser = "user";
	std::string prefix = emNick + "!" + emUser + "@localhost";
	std::string out = ":" + prefix + " KICK " + channelName + " " + targetNick;
	if (!comment.empty())
		out = out + " :" + comment;
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

	channel.removeClient(targetFd);
	if (channel.clients.empty())
		_channels.erase(it);
}

// "INVITE <nick> <#channel>"
// nc: INVITE <nick> <#channel>
// irssi: /invite <nick> <#channel>
void	Server::handleInviteCommand(int clientFd, const std::string &line)
{
	const size_t prefix_len = 7;
	if (line.size() <= prefix_len)
	{
		sendNumericMessage(clientFd, "461 INVITE :Not enough parameters");
		return;
	}
	size_t space = line.find(' ', prefix_len);
	if (space == std::string::npos)
	{
		sendNumericMessage(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	std::string targetNick = line.substr(prefix_len, space - prefix_len);
	std::string channelName;
	size_t channelNameStart = space + 1;
	if (channelNameStart >= line.size())
	{
		sendNumericMessage(clientFd, "461 INVITE :Not enough parameters");
		return;
	}
	if (line[channelNameStart] == ':')
		++channelNameStart;

	channelName = line.substr(channelNameStart);
	if (!channelName.empty() && channelName[channelName.size() - 1] == '\r')
		channelName.erase(channelName.size() - 1, 1);

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
		sendNumericMessage(clientFd, "461 INVITE :Not enough parameters");
		return;
	}

	int targetFd = findFdByNick(targetNick);
	if (targetFd == -1)
	{
		sendNumericMessage(clientFd, "401 " + targetNick + " :No such nick");
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

	if (!channel.isClientOperator(clientFd))
	{
		sendNumericMessage(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

	if (channel.isClientInChannel(targetFd))
	{
		sendNumericMessage(clientFd, "443 " + targetNick + " " + channelName + " :is already on channel"); // ERR_USERONCHANNEL 443
		return;
	}
	std::string emNick = _clients[clientFd].nickname;
	std::string emUser = _clients[clientFd].username;
	if (emNick.empty())
		emNick = convertIntToString(clientFd);
	if (emUser.empty())
		emUser = "user";
	std::string prefix = emNick + "!" + emUser + "@localhost";
	std::string inviteMsg = ":" + prefix + " INVITE " + targetNick + " " + channelName;
	sendNumericMessage(targetFd, inviteMsg);
	channel.addInvitedClient(targetFd);
	sendNumericMessage(clientFd, "341 " + emNick + " " + targetNick + " " + channelName);

	std::cout << "INVITE: fd " << clientFd << " invite to " << targetNick << " for " << channelName << "\n";
}

// "TOPIC <#channel>" -> see topic
// "TOPIC <#channel> :<new topic>" -> change topic
// nc:    TOPIC <#channel>
// nc:    TOPIC <#channel> :<new topic>
// irssi: /topic <#channel>
// irssi: /topic <#channel> :<new topic>
void Server::handleTopicCommand(int clientFd, const std::string &line)
{
	const size_t prefix_len = 6;
	if (line.size() <= prefix_len)
	{
		sendNumericMessage(clientFd, "461 TOPIC :Not enough parameters");
		return;
	}
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

	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

	if (channelName.empty())
	{
		sendNumericMessage(clientFd, "461 TOPIC :Not enough parameters");
		return;
	}
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		sendNumericMessage(clientFd, "403 " + channelName + " :No such channel"); // ERR_NOSUCHCHANNEL 403
		return;
	}
	Channel &channel = it->second;
	if (!channel.isClientInChannel(clientFd))
	{
		sendNumericMessage(clientFd, "442 " + channelName + " :You're not on that channel"); // ERR_NOTONCHANNEL 442
		return;
	}
	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;
	if (nick.empty())
		nick = convertIntToString(clientFd);
	if (user.empty())
		user = "user";
	std::string prefix = nick + "!" + user + "@localhost";
	if (rest.empty())
	{
		if (channel.hasTopic())
		{
			std::string reply = ":ircserv 332 " + nick + " " + channelName + " :" + channel.getTopic(); // RPL_TOPIC 332
			sendNumericMessage(clientFd, reply);
		}
		else
		{
			std::string reply = ":ircserv 331 " + nick + " " + channelName + " :No topic is set"; // RPL_NOTOPIC 331
			sendNumericMessage(clientFd, reply);
		}
		return;
	}
	std::string newTopic = rest;
	if (!newTopic.empty() && newTopic[0] == ':')
		newTopic.erase(0, 1);
	if (channel.isTopicRestricted() && !channel.isClientOperator(clientFd))
	{
		sendNumericMessage(clientFd, "482 " + channelName + " :You're not channel operator"); // ERR_CHANOPRIVSNEEDED 482
		return;
	}

	channel.setTopic(newTopic, nick);
	std::string out = ":" + prefix + " TOPIC " + channelName + " :" + newTopic;
	std::set<int>::iterator sit = channel.clients.begin();
	while (sit != channel.clients.end())
	{
		int fd = *sit;
		if (_clients.find(fd) != _clients.end())
			sendNumericMessage(fd, out);
		++sit;
	}
}

void Server::handleChannelModes(int clientFd, const std::string &line)
{
	const size_t prefix_len = 5;
	if (line.size() <= prefix_len)
	{
		sendNumericMessage(clientFd, "461 MODE :Not enough parameters");
		return;
	}

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

	while (!channelName.empty() && (channelName[0] == ' ' || channelName[0] == '\t'))
		channelName.erase(0, 1);
	while (!channelName.empty() && (channelName[channelName.size() - 1] == ' ' || channelName[channelName.size() - 1] == '\t'))
		channelName.erase(channelName.size() - 1, 1);

	if (channelName.empty())
	{
		sendNumericMessage(clientFd, "461 MODE :Not enough parameters");
		return;
	}

	if (channelName[0] != '#' && channelName[0] != '&' && channelName[0] != '+' && channelName[0] != '!')
	{
		mode_user(clientFd, channelName, rest);
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

	std::string nick = _clients[clientFd].nickname;
	std::string user = _clients[clientFd].username;
	if (nick.empty())
		nick = convertIntToString(clientFd);
	if (user.empty())
		user = "user";
	std::string prefix = nick + "!" + user + "@localhost";

// To know which modes are activated
	if (rest.empty())
	{
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
		std::string reply = ":ircserv 324 " + nick + " " + channelName + " +" + modes + params;
		sendNumericMessage(clientFd, reply);
		return;
	}

	handleModeChange(clientFd, channel, channelName, rest, nick, prefix);
}

void Server::handleModeChange(int clientFd, Channel &channel, const std::string &channelName, const std::string &rest, const std::string &nick, const std::string &prefix)
{
	std::vector<std::string> tokens;
	std::string s = rest;
	while (!s.empty())
	{
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
		sendNumericMessage(clientFd, "461 MODE :Not enough parameters");
		return;
	}

	std::string modeToken = tokens[0];
	size_t paramIndex = 1;
	bool plus = true;
	bool onlyB = true;
	bool hasPlusSign = false;
	size_t k = 0;
	while (k < modeToken.size())
	{
		if (modeToken[k] == '+')
		{
			hasPlusSign = true;
			++k;
			continue;
		}
		if (modeToken[k] == '-')
		{
			++k;
			continue;
		}
		if (modeToken[k] != 'b')
		{
			onlyB = false;
			break;
		}
		++k;
	}

	if (!channel.isClientOperator(clientFd))
	{
		if (onlyB && hasPlusSign)
			return;
		sendNumericMessage(clientFd, "482 " + channelName + " :You're not channel operator");
		return;
	}

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
					sendNumericMessage(clientFd, "461 MODE :Not enough parameters");
					return;
				}
				mode_k(clientFd, channel, channelName, plus, tokens[paramIndex++], prefix);
			}
			else
				mode_k(clientFd, channel, channelName, plus, std::string(""), prefix);
		}
		else if (modeToken[i] == 'l')
		{
			if (plus)
			{
				if (paramIndex >= tokens.size())
				{
					sendNumericMessage(clientFd, "461 MODE :Not enough parameters");
					return;
				}
				mode_l(clientFd, channel, channelName, plus, tokens[paramIndex++], prefix);
			}
			else
				mode_l(clientFd, channel, channelName, plus, std::string(""), prefix);
		}
		else if (modeToken[i] == 'o')
		{
			if (paramIndex >= tokens.size())
			{
				sendNumericMessage(clientFd, "461 MODE :Not enough parameters");
				return;
			}
			mode_o(clientFd, channel, channelName, plus, tokens[paramIndex++], prefix);
		}
		else
		{
			std::string unknown = ":ircserv 472 " + nick + " " + std::string(1, modeToken[i]) + " :is unknown mode char to me"; //RPL_UNKNOWNMODE 472
			sendNumericMessage(clientFd, unknown);
		}
		++i;
	}
}
