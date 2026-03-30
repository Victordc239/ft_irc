/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 13:51:15 by sofernan          #+#    #+#             */
/*   Updated: 2026/03/30 13:51:18 by sofernan         ###   ########.fr       */
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