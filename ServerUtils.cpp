/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerUtils.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sofernan <sofernan@student.42madrid.es>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/10 16:21:40 by vdiez-cu          #+#    #+#             */
/*   Updated: 2026/03/19 16:58:25 by sofernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

std::string intToString(int n)
{
	if (n == 0)
		return ("0");

	std::string result;

	while (n > 0)
	{
		char digit = '0' + (n % 10);
		result.insert(result.begin(), digit);
		n /= 10;
	}

	return (result);
}

long ft_strtol(const char *str, char **endptr)
{
	if (!str)
	{
		if (endptr)
			*endptr = NULL;
		return (0);
	}

	const char *s = str;
	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f')
		s++; // saltar espacios iniciales

	int sign = 1;
	if (*s == '+')
		s++;
	else if (*s == '-')
	{
		sign = -1;
		s++;
	}

	long result = 0;
	bool any_digit = false;

	while (*s >= '0' && *s <= '9')
	{
		int digit = *s - '0';
		if (result > (LONG_MAX - digit) / 10)
		{
			if (sign == 1)
				result = LONG_MAX;
			else
				result = LONG_MIN;
			any_digit = true;
			// consumir los dígitos restantes
			while (*s >= '0' && *s <= '9')
				s++;
			break;
		}

		result = result * 10 + digit;
		any_digit = true;
		s++;
	}

	if (endptr)
	{
		if (any_digit)
			*endptr = (char *)s;
		else
			*endptr = (char *)str;
	}

	return (result * sign);
}

bool	parseDccIpToken(const std::string &tok, struct in_addr &out)
{
	// Caso 1: IP decimal estilo DCC (ej: 2130706433)
	char *endptr = NULL;
	unsigned long ip_dec = strtoul(tok.c_str(), &endptr, 10);
	if (!tok.empty() && *endptr == '\0')
	{
		out.s_addr = htonl((uint32_t)ip_dec);
		return (true);
	}

	// Caso 2: IP normal con puntos (ej: 127.0.0.1)
	out.s_addr = inet_addr(tok.c_str());
	if (out.s_addr == INADDR_NONE && tok != "255.255.255.255")
		return (false);

	return (true);
}
