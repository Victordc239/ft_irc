/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:57 by victor            #+#    #+#             */
/*   Updated: 2026/03/19 16:41:58 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel()
{
	name = "";
	clients.clear();
	operators.clear();
	topic = "";
	topic_set_by = "";
	topic_set_time = 0;
	topic_restricted = true;
	invite_only = false;
	key = "";
	limit = 0;
}

Channel::Channel(const std::string &channel_name)
{
	name = channel_name;
	clients.clear();
	operators.clear();
	topic = "";
	topic_set_by = "";
	topic_set_time = 0;
	topic_restricted = true;
	invite_only = false;
	key = "";
	limit = 0;
}

Channel::Channel(const Channel &other)
{
	name = other.name;
	clients = other.clients;
	operators = other.operators;
	topic = other.topic;
	topic_set_by = other.topic_set_by;
	topic_set_time = other.topic_set_time;
	topic_restricted = other.topic_restricted;
	invite_only = other.invite_only;
	key = other.key;
	limit = other.limit;
}

Channel &Channel::operator=(const Channel &other)
{
	if (this != &other)
	{
		name = other.name;
		clients = other.clients;
		operators = other.operators;
		topic = other.topic;
		topic_set_by = other.topic_set_by;
		topic_set_time = other.topic_set_time;
		topic_restricted = other.topic_restricted;
		invite_only = other.invite_only;
		key = other.key;
		limit = other.limit;
	}
	return (*this);
}

Channel::~Channel()
{
	
}

void Channel::addClient(int fd)
{
	clients.insert(fd);
}

void Channel::removeClient(int fd)
{
	clients.erase(fd);
	// si era operador, quitarlo también
	operators.erase(fd);
}

bool Channel::hasClient(int fd) const
{
	return (clients.find(fd) != clients.end());
}

void Channel::addOperator(int fd)
{
	operators.insert(fd);
}

void Channel::removeOperator(int fd)
{
	operators.erase(fd);
}

bool Channel::isOperator(int fd) const
{
	return (operators.find(fd) != operators.end());
}

// setTopic guarda texto, quien lo puso (nickname) y la hora actual
void Channel::setTopic(const std::string &newTopic, const std::string &setter)
{
	topic = newTopic;
	topic_set_by = setter;
	topic_set_time = std::time(NULL);
}

// indica si hay topic (topic != "")
bool Channel::hasTopic() const
{
	return !(topic.empty());
}

const std::string &Channel::getTopic() const
{
	return (topic);
}

const std::string &Channel::getTopicSetter() const
{
	return (topic_set_by);
}

time_t Channel::getTopicTime() const
{
	return (topic_set_time);
}

void Channel::setTopicRestricted(bool v)
{
	topic_restricted = v;
}

bool Channel::isTopicRestricted() const
{
	return (topic_restricted);
}

void Channel::setInviteOnly(bool v)
{
	invite_only = v;
}

bool Channel::isInviteOnly() const
{
	return (invite_only);
}

void Channel::setKey(const std::string &k)
{
	key = k;
}

bool Channel::hasKey() const
{
	return !(key.empty());
}

const std::string &Channel::getKey() const
{
	return (key);
}

void Channel::removeKey()
{
	key.clear();
}

void Channel::setLimit(int n)
{
	limit = n;
}

int Channel::getLimit() const
{
	return (limit);
}

void Channel::removeLimit()
{
	limit = 0;
}

bool Channel::isInvited(int fd) const
{
	return (invitedClients.find(fd) != invitedClients.end());
}

void Channel::addInvite(int fd)
{
	invitedClients.insert(fd);
}

void Channel::removeInvite(int fd)
{
	invitedClients.erase(fd);
}
