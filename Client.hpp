/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/04 13:54:02 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/04 17:35:52 by vdiez-cu         ###   ########.fr       */
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
		std::string nick;		// nickname
		std::string user;		// username
		std::string realname;	// parte después de ':' en USER ejemplo: [USER vdiez-cu vdiez-cu localhost :Victor Diez Cuesta]
		bool registered;		// true cuando NICK+USER procesados

		Client();
		Client(int fd_client);
		Client(const Client &other);
		Client &operator=(const Client &other);
		~Client();
};

#endif