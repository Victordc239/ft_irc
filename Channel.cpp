/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:57 by victor            #+#    #+#             */
/*   Updated: 2026/03/09 17:27:53 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel()
{
	this->name = "";
	this->clients.clear();
	this->operators.clear();
	this->topic = "";
	this->topic_set_by = "";
	this->topic_set_time = 0;
	this->topic_restricted = false;
}

Channel::Channel(const std::string &channel_name)
{
	this->name = channel_name;
	this->clients.clear();
	this->operators.clear();
	this->topic = "";
	this->topic_set_by = "";
	this->topic_set_time = 0;
	this->topic_restricted = false;
}

Channel::Channel(const Channel &other)
{
	this->name = other.name;
	this->clients = other.clients;
	this->operators = other.operators;
	this->topic = other.topic;
	this->topic_set_by = other.topic_set_by;
	this->topic_set_time = other.topic_set_time;
	this->topic_restricted = other.topic_restricted;
}

Channel &Channel::operator=(const Channel &other)
{
	if (this != &other)
	{
		this->name = other.name;
		this->clients = other.clients;
		this->operators = other.operators;
		this->topic = other.topic;
		this->topic_set_by = other.topic_set_by;
		this->topic_set_time = other.topic_set_time;
		this->topic_restricted = other.topic_restricted;
	}
	return *this;
}

Channel::~Channel()
{
	
}

void Channel::addClient(int fd)
{
	this->clients.insert(fd);
}

void Channel::removeClient(int fd)
{
	this->clients.erase(fd);
	// si era operador, quitarlo también
	this->operators.erase(fd);
}

bool Channel::hasClient(int fd) const
{
	return (this->clients.find(fd) != this->clients.end());
}

void Channel::addOperator(int fd)
{
	this->operators.insert(fd);
}

void Channel::removeOperator(int fd)
{
	this->operators.erase(fd);
}

bool Channel::isOperator(int fd) const
{
	return (this->operators.find(fd) != this->operators.end());
}

// setTopic guarda texto, quien lo puso (nickname) y la hora actual
void Channel::setTopic(const std::string &newTopic, const std::string &setter)
{
	this->topic = newTopic;
	this->topic_set_by = setter;
	this->topic_set_time = std::time(NULL);
}

// indica si hay topic (topic != "")
bool Channel::hasTopic() const
{
	return !this->topic.empty();
}

const std::string &Channel::getTopic() const
{
	return this->topic;
}

const std::string &Channel::getTopicSetter() const
{
	return this->topic_set_by;
}

time_t Channel::getTopicTime() const
{
	return this->topic_set_time;
}

void Channel::setTopicRestricted(bool v)
{
	this->topic_restricted = v;
}

bool Channel::isTopicRestricted() const
{
	return this->topic_restricted;
}
