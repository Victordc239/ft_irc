/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:11 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/12 16:30:55 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client()
{
	fd = -1;
	accum = "";
	outbuf = "";
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
	accum = "";
	outbuf = "";
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
	accum = other.accum;
	outbuf = other.outbuf;
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
		accum = other.accum;
		outbuf = other.outbuf;
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
