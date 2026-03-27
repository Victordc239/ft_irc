/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 15:06:23 by sofernan          #+#    #+#             */
/*   Updated: 2026/03/27 16:29:27 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Bot.hpp"

Bot::Bot()
{
	_name = "bot";
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
		return ("Hello " + nick + " 👋");

	if (cmd == "!help")
		return ("Commands: !hello !time");

	if (cmd == "!time")
	{
		time_t now = time(NULL);
		std::string t = ctime(&now);
		t.erase(t.size() - 1); // remove the final '\n'
		return ("Server time: " + t);
	}
	
	return ("");
}
