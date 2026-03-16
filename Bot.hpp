/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/16 15:04:46 by sofernan          #+#    #+#             */
/*   Updated: 2026/03/16 18:20:35 by sofernan         ###   ########.fr       */
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
		~Bot();

		const std::string &getName() const;
		std::string generateReply(const std::string &cmd, const std::string &nick);
};

#endif
