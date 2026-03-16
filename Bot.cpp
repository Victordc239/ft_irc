/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 15:06:23 by sofernan          #+#    #+#             */
/*   Updated: 2026/03/16 17:11:23 by sofernan         ###   ########.fr       */
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
        std::string t = ctime(&now);
        t.erase(t.size() - 1); // quita el '\n' final
        return ("Server time: " + t);
    }
    
    if (cmd.find("!kick ") == 0)
    {
        kickTarget = cmd.substr(6);
        return ("BOT_KICK" + kickTarget);
    }

    return ("");
}
