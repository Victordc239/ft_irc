/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:11 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/04 22:10:16 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client()
{
	this->fd = -1;
	this->accum = "";
	this->outbuf = "";
	this->correctPass = false;
	this->nickname = "";
	this->username = "";
	this->realname = "";
	this->registered = false;
}

Client::Client(int fd_client)
{
	this->fd = fd_client;
	this->accum = "";
	this->outbuf = "";
	this->correctPass = false;
	this->nickname = "";
	this->username = "";
	this->realname = "";
	this->registered = false;
}

Client::Client(const Client &other)
{
	this->fd = other.fd;
	this->accum = other.accum;
	this->outbuf = other.outbuf;
	this->correctPass = other.correctPass;
	this->nickname = other.nickname;
	this->username = other.username;
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
		this->nickname = other.nickname;
		this->username = other.username;
		this->realname = other.realname;
		this->registered = other.registered;
	}
	return *this;
}

Client::~Client()
{

}
