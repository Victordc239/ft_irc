/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:48 by victor            #+#    #+#             */
/*   Updated: 2026/03/09 14:11:16 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>

class Channel
{
	public:
		std::string name;
		std::set<int> clients;	// fds de clientes dentro del canal. SET es un container que ya te introduce los valores ordenados
		std::set<int> operators;  // fds de operadores del canal

		Channel();
		Channel(const std::string &channel_name);
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);
		~Channel();

		void addClient(int fd);
		void removeClient(int fd);
		bool hasClient(int fd) const;
		void addOperator(int fd);
		void removeOperator(int fd);
		bool isOperator(int fd) const;
};

#endif