/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vdiez-cu <vdiez-cu@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:48 by victor            #+#    #+#             */
/*   Updated: 2026/03/10 14:39:17 by vdiez-cu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <ctime>

class Channel
{
	public:
		std::string name;
		std::set<int> clients;		// fds de clientes dentro del canal. SET es un container que ya te introduce los valores ordenados
		std::set<int> operators;	// fds de operadores del canal
		std::string topic;		// texto del topic (vacío = no hay topic)
		std::string topic_set_by;	// nickname que puso el topic
		time_t topic_set_time;		// momento  en el qie se puso el topic (0 = no topic)
		bool topic_restricted;		// si true -> sólo operadores pueden cambiar topic (mode +t)
		bool invite_only;			// operator puede invitar a un canal mode +i
		std::string key;			// operator puede poner y quitar contraseña del canal mode +k (vacío = sin key)
		int limit;				// operator puede poner limite de usuarios a canal mode +l (0 = sin límite)

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
		void setTopic(const std::string &newTopic, const std::string &setter);
		bool hasTopic() const;
		const std::string &getTopic() const;
		const std::string &getTopicSetter() const;
		time_t getTopicTime() const;
		void setTopicRestricted(bool v);
		bool isTopicRestricted() const;
		void setInviteOnly(bool v);
		bool isInviteOnly() const;
		void setKey(const std::string &k);
		bool hasKey() const;
		const std::string &getKey() const;
		void removeKey();
		void setLimit(int n);
		int getLimit() const;
		void removeLimit();
};

#endif