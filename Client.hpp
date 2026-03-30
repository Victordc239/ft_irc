/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:02 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/30 09:54:12 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
	public:
		int fd;
		std::string accumulator;
		std::string outBuffer;
		bool correctPass;
		std::string nickname;
		std::string username;
		std::string realname;
		bool registered;
		bool invisible;

		Client();
		Client(int fd_client);
		Client(const Client &other);
		Client &operator=(const Client &other);
		~Client();
};

#endif