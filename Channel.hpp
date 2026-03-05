/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:48 by victor            #+#    #+#             */
/*   Updated: 2026/03/05 15:59:25 by vdiez-cu         ###   ########.fr       */
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
		std::set<int> clients;	// fds de clientes dentro del canal
		int operator_fd;		// operador del canal

		Channel();
		Channel(const std::string &channel_name);
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);
		~Channel();

		void addClient(int fd);
		void removeClient(int fd);
		bool hasClient(int fd) const;
};

#endif