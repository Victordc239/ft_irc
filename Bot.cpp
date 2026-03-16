/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 15:06:23 by sofernan          #+#    #+#             */
/*   Updated: 2026/03/16 22:23:08 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Bot.hpp"

Bot::Bot()
{
	_name = "irc_bot";
}

Bot::Bot(const Bot &other)
{
	_name = other._name;
}

Bot &Bot::operator=(const Bot &other)
{
	if (this != &other)
	{
		_name = other._name;
	}
	return (*this);
}

Bot::~Bot()
{

}

const std::string &Bot::getName() const
{
	return (_name);
}

std::string Bot::generateReply(const std::string &cmd, const std::string &nick)
{
	if (cmd == "!hello")
		return ("Hello" + nick + "👋");

	if (cmd == "!help")
		return ("Commands: !hello !time");

	if (cmd == "!time")
	{
		time_t now = time(NULL);
		std::string t = ctime(&now);
		t.erase(t.size() - 1); // quita el '\n' final
		return ("Server time: " + t);
	}
	
	return ("");
}
