*This project has been created as part of the 42 curriculum by sofernan, vdiez-cu and alejaro2.*

# ft_irc

## 📝 Description 
`ft_irc` is a C++ project that implements a functional IRC server following the IRC (Internet Relay Chat) protocol.  

This project allows multiple clients to connect to the server, join channels, send private and public messages, and interact in real time, effectively simulating the behavior of a real IRC server. The server is built using TCP sockets and handles multiple clients using I/O multiplexing.

The project’s main goals are:
- Develop and apply socket programming in C++.
- Manage multiple client connections concurrently.   
- Understand and implement the IRC protocol.
- Manage channels, nicknames, and messaging (both public and private).

The purpose of this project is to gain hands-on experience with network programming, socket management, and real-time communication by building a working IRC server from scratch.  

Additionally, the `bonus part` of the project has been implemented to extend the server’s functionality and make it closer to a real IRC server. These extra features include:
- File transfer support between clients using DCC through a server-side proxy mechanism.
- Implementation of a bot that automatically handles and responds to specific private or channel messages.

## 🌐 Architecture Overview
```text
+----------------+        TCP        +----------------+
|  IRC Client 1  | <--------------> |                |
+----------------+                   |                |
|  IRC Client 2  | <--------------> |   IRC Server   |
+----------------+                   |                |
|  IRC Client 3  | <--------------> |                |
+----------------+                   +----------------+
```
- The server listens on a specified port and manages incoming TCP connections.  
- Each client connects via TCP sockets and can send commands/messages.  
- The server manages multiple clients simultaneously using `poll()`.  
- Bonus file transfers are handled through additional sockets and a server-side DCC proxy.

## 💡 Features

### Mandatory Part
- Multi-channel support with unique channel names  
- Public messages broadcasted to all clients in a channel  
- Private messaging between individual clients  
- Nickname registration and validation  
- Password-protected server access  
- Graceful handling of client disconnections  
- Command parsing following IRC standards
- Support for core IRC commands (NICK, USER, JOIN, PRIVMSG, KICK, INVITE, TOPIC, MODE, PART, PASS, CAP, PING, PONG, QUIT)

### Bonus Part
- File transfer between clients using DCC (DCC SEND / DCC GET)  
- IRC bot implementation for automated responses  

## 🖥️ Supported IRC Commands
- `NICK`: NICK command is used to give user a nickname or change the existing one.
- `USER`: The USER command is used during connection setup to provide the username and real name of a new client.
- `JOIN`: The JOIN command is used by a user to request to start listening to the specific channel.
- `PRIVMSG`: PRIVMSG is used to send private messages between users, as well as to send messages to channels.
- `KICK`: The KICK command can be used to request the forced removal of a user from a channel.
- `INVITE`: The INVITE command allows a channel operator to invite another user to a channel, especially when the channel is set to invite-only mode.
- `TOPIC`: The TOPIC command is used to change or view the topic of a channel.
- `MODE`: The MODE command is used to manage channel modes and permissions, such as invite-only access, topic restrictions, passwords, user limits, and operator privileges.
- `PART`: The PART command causes the user sending the message to be removed from the list of active members for all given channels listed in the parameter string.
- `PASS`: The PASS command is used to set a 'connection password'.
- `CAP`: The CAP command is used to manage the IRC client’s capabilities.
- `PING`: The PING command is used to check that the connection is still active, and the server responds to prevent the client from disconnecting due to timeout.
- `PONG`: The PONG command is the response to a PING, and it is used to confirm that the client is still connected and to keep the connection alive.
- `QUIT`: A client session is terminated with a quit message.
- `DCC SEND`: The DCC SEND command is used to initiate a file transfer between users.
- `DCC GET`: The DCC GET is used to receive a file from another user via the DCC (Direct Client-to-Client) protocol.

## ⚙️ Instructions

### Requirements
- C++ compiler supporting C++98
- `make`
- Linux operating system 
- Basic knowledge of terminal commands and networking

### Clone the Repository
First, clone the project repository and navigate into it:
```bash
git clone <repository_url>
```

### Compilation
To compile the program, use:
```bash
make
```
The project is compiled with `-Wall -Wextra -Werror` and `-std=c++98` as required by the subject.

### Running the Server
To start the Server, use:
```bash
./ircserv <port> <password>
```
- `port`: The port number on which your IRC server will be listening to for incoming IRC connections.
- `password`: The password required for clients to connect and authenticate with the server.

### Connecting to the Server
You can connect to the server using a terminal-based client such as `netcat` or a standard IRC client such as `Irssi`.

#### Using netcat:
```bash
nc <IP_address> <port>
```
#### Using irssi:
```bash
irssi -c <ip_address> -p <port>
```
- `IP_address`: The IP address of the host running the server.
- `port`: The port on which the server is listening.

### Initial Authentication (IRC protocol)

```text
PASS <password>
NICK your_nickname
USER your_username
```

## 🔍 Resources
- https://www.rfc-editor.org/rfc/rfc1459.html
- https://es.wikipedia.org/wiki/Internet_Relay_Chat
- https://irssi.org/documentation/manual/

### AI Assistance
Artificial Intelligence tools were used for:
- Structuring and writing the README file.
- Clarifying IRC protocol behavior and command usage.
- Providing guidance on best practices for socket programming and project organization.

No AI was used to implement the core logic of the project.
