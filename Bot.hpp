/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: victor <victor@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 15:04:46 by sofernan          #+#    #+#             */
/*   Updated: 2026/03/30 11:17:11 by victor           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef BOT_HPP
#define BOT_HPP

#include <string>
#include <ctime>

class Bot
{
	private:
		std::string _name;

	public:
		Bot();
		Bot(const Bot &other);
		Bot &operator=(const Bot &other);
		~Bot();

		const std::string &getName() const;
		std::string generateReply(const std::string &cmd, const std::string &nick);
};

#endif
