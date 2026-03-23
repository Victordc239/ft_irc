/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:02 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/23 17:48:37 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
	public:
		int fd;
		std::string accumulator;	// Acumulador para recibir todos los mensajes completos
		std::string outBuffer;	// Buffer para enviar mensaje de servidor a cliente
		bool correctPass;		// si ha autenticado correctamente
		std::string nickname;	// nickname
		std::string username;	// username
		std::string realname;	// parte después de ':' en USER ejemplo: [USER vdiez-cu vdiez-cu localhost :Victor Diez Cuesta]
		bool registered;		// true cuando NICK+USER procesados
		bool invisible;		// modo +i de usuario (invisible)

		Client();
		Client(int fd_client);
		Client(const Client &other);
		Client &operator=(const Client &other);
		~Client();
};

#endif