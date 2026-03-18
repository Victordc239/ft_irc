*This project has been created as part of the 42 curriculum by sofernan, vdiez-cu and alejaro2.*

# ft_irc

## Description 
`ft_irc` is a C++ project that implements a fully functional IRC server and client following the IRC (Internet Relay Chat) protocol.  

This project allows multiple clients to connect to the server, join channels, send private and public messages, and interact in real time, effectively simulating the behavior of a real IRC server.

The project’s main goals are:
- Develop and apply socket programming in C++.
- Manage multiple client connections concurrently.   
- Understand and implement the IRC protocol.
- Manage channels, nicknames, and messaging (both public and private).

The purpose of this project is to gain hands-on experience with network programming, socket management, and real-time communication by building a working IRC system from scratch.  

Additionally, the bonus part of the project has been implemented to extend the server’s functionality and make it closer to a real IRC server. These extra features include:
- File transfer management between clients.  
- Implementation of a bot to automate interactions and responses. 

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

## Our IRC Commands
- `NICK`: NICK command is used to give user a nickname or change the existing one.
- `USER`: The USER command is used at the beginning of connection to specify the username, hostname and realname of a new user.
- `JOIN`: The JOIN command is used by a user to request to start listening to the specific channel.
- `PRIVMSG`: PRIVMSG is used to send private messages between users, as well as to send messages to channels.
- `KICK`: The KICK command can be used to request the forced removal of a user from a channel.
- `INVITE`:
- `TOPIC`: The TOPIC command is used to change or view the topic of a channel.
- `MODE`: The user MODE's are typically changes which affect either how the client is seen by others or what 'extra' messages the client is sent.
- `PART`: The PART command causes the user sending the message to be removed from the list of active members for all given channels listed in the parameter string.
- `PASS`: The PASS command is used to set a 'connection password'.
- `CAP`: The CAP command is used to manage the IRC client’s capabilities.
- `PING`: The PING command is used to check that the connection is still active, and the server responds to prevent the client from disconnecting due to timeout.
- `PONG`: The PONG command is the response to a `PING`, and it is used to confirm that the client is still connected and to keep the connection alive.
- `QUIT`: A client session is terminated with a quit message.
- `DCC SEND`: The DCC SEND command is used to send a file to another user.
- `DCC GET`: The DCC GET command is used to receive a file from another user.

## Instructions

### Requirements
- C++ compiler supporting C++98
- `make` for building the project  
- Linux operating system 
- Basic knowledge of terminal commands and networking programming

### Clone the Repository
First, clone the project repository and navigate into it:
`git clone <repository_url>`

### Compilation
To compile the program, use:

make To start the Server, use:
`./ircserv <port> <password>`
-  port: The port number on which your IRC server will be listening to for incoming IRC connections.
-  password: The connection password. It will be needed by any IRC client that tries to connect to your server. To connect to the server, you can use:
`nc <IP ADDRESS> <PORT>`
-  IP ADDRESS: Host IP address.
-  PORT: The PORT that the server listening on.
You can also use the Irssi IRC client.

## Resources
- RFC 1459 – Internet Relay Chat Protocol  
- Beej’s Guide to Network Programming  
- https://beej.us/guide/bgnet/  
- https://www.rfc-editor.org/rfc/rfc1459.html  
- https://en.cppreference.com/

### AI Assistance
Artificial Intelligence was used for:
- Structuring and writing the README file.
- Clarifying IRC protocol behavior and command usage.
- Providing guidance on best practices for socket programming and project organization.

No AI was used to implement the core logic of the project.
