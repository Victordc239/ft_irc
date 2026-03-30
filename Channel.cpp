/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:57 by victor            #+#    #+#             */
/*   Updated: 2026/03/30 09:53:45 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel()
{
	name = "";
	clients.clear();
	operators.clear();
	topic = "";
	topicSetBy = "";
	topicSetTime = 0;
	topicRestricted = true;
	inviteOnly = false;
	key = "";
	limit = 0;
}

Channel::Channel(const std::string &channel_name)
{
	name = channel_name;
	clients.clear();
	operators.clear();
	topic = "";
	topicSetBy = "";
	topicSetTime = 0;
	topicRestricted = true;
	inviteOnly = false;
	key = "";
	limit = 0;
}

Channel::Channel(const Channel &other)
{
	name = other.name;
	clients = other.clients;
	operators = other.operators;
	topic = other.topic;
	topicSetBy = other.topicSetBy;
	topicSetTime = other.topicSetTime;
	topicRestricted = other.topicRestricted;
	inviteOnly = other.inviteOnly;
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
		topicSetBy = other.topicSetBy;
		topicSetTime = other.topicSetTime;
		topicRestricted = other.topicRestricted;
		inviteOnly = other.inviteOnly;
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
	operators.erase(fd);
}

bool Channel::isClientInChannel(int fd) const
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

bool Channel::isClientOperator(int fd) const
{
	return (operators.find(fd) != operators.end());
}

// Save the text, who posted it (nickname), and the current time
void Channel::setTopic(const std::string &newTopic, const std::string &setter)
{
	topic = newTopic;
	topicSetBy = setter;
	topicSetTime = std::time(NULL);
}

bool Channel::hasTopic() const
{
	return !(topic.empty());
}

const std::string &Channel::getTopic() const
{
	return (topic);
}

void Channel::setTopicRestricted(bool v)
{
	topicRestricted = v;
}

bool Channel::isTopicRestricted() const
{
	return (topicRestricted);
}

void Channel::setInviteOnly(bool v)
{
	inviteOnly = v;
}

bool Channel::isInviteOnly() const
{
	return (inviteOnly);
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

void Channel::setUserLimit(int n)
{
	limit = n;
}

int Channel::getUserLimit() const
{
	return (limit);
}

void Channel::removeUserLimit()
{
	limit = 0;
}

bool Channel::isClientInvited(int fd) const
{
	return (invitedClients.find(fd) != invitedClients.end());
}

void Channel::addInvitedClient(int fd)
{
	invitedClients.insert(fd);
}

void Channel::removeInvitedClient(int fd)
{
	invitedClients.erase(fd);
}
