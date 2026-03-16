/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 15:06:23 by sofernan          #+#    #+#             */
/*   Updated: 2026/03/16 18:20:29 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Bot.hpp"

Bot::Bot() : _name("irc_bot") {}

Bot::~Bot() {}

const std::string &Bot::getName() const
{
    return (_name);
}

std::string Bot::generateReply(const std::string &cmd, const std::string &nick)
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
    
    return ("");
}
