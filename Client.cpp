/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:11 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/04 17:38:41 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client()
{
	this->fd = -1;
	this->accum = "";
	this->outbuf = "";
	this->correctPass = false;
	this->nick = "";
	this->user = "";
	this->realname = "";
	this->registered = false;
}

Client::Client(int fd_client)
{
	this->fd = fd_client;
	this->accum = "";
	this->outbuf = "";
	this->correctPass = false;
	this->nick = "";
	this->user = "";
	this->realname = "";
	this->registered = false;
}

Client::Client(const Client &other)
{
	this->fd = other.fd;
	this->accum = other.accum;
	this->outbuf = other.outbuf;
	this->correctPass = other.correctPass;
	this->nick = other.nick;
	this->user = other.user;
	this->realname = other.realname;
	this->registered = other.registered;
}

Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		this->fd = other.fd;
		this->accum = other.accum;
		this->outbuf = other.outbuf;
		this->correctPass = other.correctPass;
		this->nick = other.nick;
		this->user = other.user;
		this->realname = other.realname;
		this->registered = other.registered;
	}
	return *this;
}

Client::~Client()
{

}
