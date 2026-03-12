/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:02 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/12 15:03:42 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
	public:
		int fd;
		std::string accum;	// Acumulador para recibir todos los mensajes completos
		std::string outbuf;	// Buffer para enviar mensaje de servidor a cliente
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