*This project has been created as part of the 42 curriculum by sofernan, vdiez-cu and alejaro2.*

# ft_irc

## Description 
`ft_irc` is a C++ project that implements a basic Internet Relay Chat (IRC) server and client following the IRC protocol.  

This project allows multiple clients to connect to the server, join channels, send private and public messages, and interact in real time, effectively simulating the behavior of a real IRC server.

The project’s main goals are:
- Develop and apply socket programming in C++.
- Manage multiple client connections concurrently.   
- Understand and implement the IRC protocol.
- Manage channels, nicknames, and messaging (both public and private)

The purpose of this project is to gain hands-on experience with network programming, socket management, and real-time communication by building a working IRC system from scratch.  

## Architecture Overview
   +----------------+        TCP        +----------------+
   |  IRC Client 1  | <--------------> |                |
   +----------------+                   |                |
   |  IRC Client 2  | <--------------> |   IRC Server   |
   +----------------+                   |                |
   |  IRC Client 3  | <--------------> |                |
   +----------------+                   +----------------+

- Server listens on a specified port and manages all client connections.  
- Each client connects via TCP sockets and can send commands/messages.  
- Server handles multiple clients simultaneously.  

## Features
- Multi-channel support with unique channel names  
- Public messages broadcasted to all clients in a channel  
- Private messaging between individual clients  
- Nickname registration and validation  
- Password-protected server access  
- Graceful handling of client disconnections  
- Command parsing following IRC standards  
- Supports `NICK`, `USER`, `JOIN`, `PRIVMSG`, `KICK`, `INVITE`, `TOPIC`, `MODE`, `PART`, `PASS`, `CAP`, `PING`, `PONG`

## Instructions

### Requirements
- C++ compiler supporting C++98
- `make` for building the project  
- Linux operating system 
- Basic knowledge of terminal commands and networking programming

### Clone the Repository
First, clone the project repository and navigate into it:
```bash
git clone <repository_url>

### Compilation
To compile the project, use the following command:
```bash
c++ -Wall -Wextra -Werror -std=c++98 -o ircserv srcs/*.cpp
c++ -Wall -Wextra -Werror -std=c++98 -o ircclient srcs/*.cpp

### Steps
1. Clone the repository:
```bash
git clone <repository_url>
cd ft_irc

## Resources
documentación, artículos, tutoriales, así como una descripción de cómo se utilizó la IA, especificando para qué tareas y en qué partes del proyecto.