/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:11 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/23 17:48:37 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client()
{
	fd = -1;
	accumulator = "";
	outBuffer = "";
	correctPass = false;
	nickname = "";
	username = "";
	realname = "";
	registered = false;
	invisible = false;
}

Client::Client(int fd_client)
{
	fd = fd_client;
	accumulator = "";
	outBuffer = "";
	correctPass = false;
	nickname = "";
	username = "";
	realname = "";
	registered = false;
	invisible = false;
}

Client::Client(const Client &other)
{
	fd = other.fd;
	accumulator = other.accumulator;
	outBuffer = other.outBuffer;
	correctPass = other.correctPass;
	nickname = other.nickname;
	username = other.username;
	realname = other.realname;
	registered = other.registered;
	invisible = other.invisible;
}

Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		fd = other.fd;
		accumulator = other.accumulator;
		outBuffer = other.outBuffer;
		correctPass = other.correctPass;
		nickname = other.nickname;
		username = other.username;
		realname = other.realname;
		registered = other.registered;
		invisible = other.invisible;
	}
	return (*this);
}
Client::~Client()
{

}
