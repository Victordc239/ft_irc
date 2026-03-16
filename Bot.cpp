/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 15:06:23 by sofernan          #+#    #+#             */
/*   Updated: 2026/03/16 16:54:42 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Bot.hpp"

Bot::Bot() : _name("irc_bot") {}

Bot::~Bot() {}

const std::string &Bot::getName() const
{
    return (_name);
}

std::string Bot::generateReply(const std::string &cmd, const std::string &nick, const std::string &channel, std::string &kickTarget)
{
    if (cmd == "!hello")
        return ("Hello" + nick + "👋");

    if (cmd == "!help")
        return ("Commands: !hello !time !kick <nick>");

    if (cmd == "!time")
    {
        time_t now = time(NULL);
        return (std::string("Server time: ") + ctime(&now));
    }
    if (cmd.find("!kick ") == 0)
    {
        std::string nick = cmd.substr(6);
        return ("BOT_KICK " + nick);
    }

    return ("");
}
