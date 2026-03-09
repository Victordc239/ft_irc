/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:57 by victor            #+#    #+#             */
/*   Updated: 2026/03/09 14:12:27 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel()
{
	this->name = "";
	this->clients.clear();
	this->operators.clear();
}

Channel::Channel(const std::string &channel_name)
{
	this->name = channel_name;
	this->clients.clear();
	this->operators.clear();
}

Channel::Channel(const Channel &other)
{
	this->name = other.name;
	this->clients = other.clients;
	this->operators = other.operators;
}

Channel &Channel::operator=(const Channel &other)
{
	if (this != &other)
	{
		this->name = other.name;
		this->clients = other.clients;
		this->operators = other.operators;
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