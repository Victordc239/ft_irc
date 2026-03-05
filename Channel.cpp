/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:57 by victor            #+#    #+#             */
/*   Updated: 2026/03/05 11:09:46 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel()
{
	this->name = "";
	this->operator_fd = -1;
}

Channel::Channel(const std::string &channel_name)
{
	this->name = channel_name;
	this->operator_fd = -1;
}

Channel::Channel(const Channel &other)
{
	this->name = other.name;
	this->clients = other.clients;
	this->operator_fd = other.operator_fd;
}

Channel &Channel::operator=(const Channel &other)
{
	if (this != &other)
	{
		this->name = other.name;
		this->clients = other.clients;
		this->operator_fd = other.operator_fd;
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
}

bool Channel::hasClient(int fd) const
{
	return (this->clients.find(fd) != this->clients.end());
}
