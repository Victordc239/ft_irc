/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 10:46:48 by victor            #+#    #+#             */
/*   Updated: 2026/03/30 13:35:26 by sofernan         ###   ########.fr       */
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
		std::set<int> clients;
		std::set<int> operators;
		std::string topic;
		std::string topicSetBy;
		time_t topicSetTime;
		bool topicRestricted;
		bool inviteOnly;
		std::string key;
		int limit;
		std::set<int> invitedClients;

		Channel();
		Channel(const std::string &channel_name);
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);
		~Channel();

		void	addClient(int fd);
		void	removeClient(int fd);
		bool	isClientInChannel(int fd) const;
		void	addOperator(int fd);
		void	removeOperator(int fd);
		bool	isClientOperator(int fd) const;
		void	setTopic(const std::string &newTopic, const std::string &setter);
		bool	hasTopic() const;
		void	setTopicRestricted(bool v);
		bool	isTopicRestricted() const;
		void	setInviteOnly(bool v);
		bool	isInviteOnly() const;
		void	setKey(const std::string &k);
		bool	hasKey() const;
		void	removeKey();
		void	setUserLimit(int n);
		int		getUserLimit() const;
		void	removeUserLimit();
		bool	isClientInvited(int fd) const;
		void	addInvitedClient(int fd);
		void	removeInvitedClient(int fd);
		const std::string &getKey() const;
		const std::string &getTopic() const;
};

#endif