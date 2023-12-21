#include "Server.hpp"
#include "Channel.hpp"
#include <fcntl.h>
#include <cstring>  // for strerror
#include <cerrno>   // for errno

Server::Server(int port, const std::string &password) {
    // setting the address family - AF_INET for IPv4
    address.sin_family = AF_INET;
    // setting the port converting port value to network byte order
    address.sin_port = htons(port);
    // setting the IP - INADDR_ANY for any network interface on the machine - converting it to network byte order.
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    // creating the main listening socket and adding it to pollFds container
    socketFd = socket(address.sin_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (socketFd == -1) {
        throw std::runtime_error("Socket error: [" + std::string(strerror(errno)) + "]");
    }
    pollfd serverPollFd;
    serverPollFd.fd = socketFd;
    serverPollFd.events = POLLIN;
    pollFds.push_back(serverPollFd);
    // binding socket to the port
    if (bind(this->socketFd, (sockaddr *) (&address), sizeof(address)) == -1) {
        throw std::runtime_error("Bind error: [" + std::string(strerror(errno)) + "]");
    }
    this->start = time(0);
    this->_password = password;
    this->serverName = "42.IRC";
    this->serverVersion = "0.1";
	initCmd();
    std::cout << "Server created: address=" << inet_ntoa(address.sin_addr)
			  << ":"
			  << ntohs(address.sin_port)
			  << " socketFD=" << socketFd
			  << " _password=" << this->_password << std::endl;
}

void Server::initCmd() {
	cmd["PRIVMSG"] = &Server::processPrivmsg;
	cmd["JOIN"] = &Server::processJoin;
	cmd["INVITE"] = &Server::processInvite;
	cmd["KICK"] = &Server::processKick;
	cmd["PART"] = &Server::processPart;
    cmd["MODE"] = &Server::processMode;
    cmd["PING"] = &Server::processPing;
	// and other commands
}

Server::~Server() {
    std::cout << "Server destroyed." << std::endl;
}


void Server::addClient(int clientSocket) {
    // Create a new Client object and insert it into the clients map
    clients.insert(std::make_pair(clientSocket, new Client()));
}

void Server::removeClient(int clientSocket) {
    // removing from _channels
   for (std::vector<Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        (*it)->removeMember(clientSocket);
    }
    // removing from pollFds
   for (std::vector<pollfd>::iterator it = pollFds.begin(); it!= pollFds.end(); ++it) {
        if (it->fd == clientSocket) {
            pollFds.erase(it);
            break;
        }
    }
    // deleting and removing from clients
    std::map<int, Client *>::iterator it = clients.find(clientSocket);
    if (it != clients.end()) {
        delete it->second;
        clients.erase(it);
    }
}


void Server::run() {
    listenPort();
    while (true) {
        int countEvents = poll(&pollFds[0], pollFds.size(), 0);

        if (countEvents < 0) {
            throw std::runtime_error("Poll error: [" + std::string(strerror(errno)) + "]");
        }
        for (size_t i = 0; i < pollFds.size(); i++) {
			if (pollFds[i].revents & POLLOUT) {
				std::cout << "Client socket has OUT events!" << std::endl;
				try {
					Client &c = getClient(pollFds[i].fd);
					while (!c.sendQueueEmpty()) {
						std::string msg = c.popSendQueue();
						std::cout << "[" << pollFds[i].fd << "] Msg to send: " << msg << std::endl;
						size_t n = send(pollFds[i].fd, msg.c_str(), msg.length(), 0);
						if (n == msg.length())
							pollFds[i].events = POLLIN;
					}
				}
				catch (std::exception &e) {
					std::cout << "Error: " << e.what() << std::endl;
				}
				pollFds[i].revents = 0;
			}
			if (pollFds[i].revents & POLLIN) {
                // if i == 0 -> first connection
                if (i == 0) {
                    addClient(acceptConnection());
                } else {
                    std::cout << "Client socket has events!" << std::endl;
                    memset(_buffer, 0, 1024);
                    int bytesRead = recv(pollFds[i].fd, _buffer, sizeof(_buffer) - 1, 0);
                    if (bytesRead > 0) {
                        _buffer[bytesRead] = 0;
                        if (parsBuffer(pollFds[i].fd)) {
//                            TODO : handle parsing errors
							(void) 0;
                        }
                    } else if (bytesRead == 0) {
                        removeClient(pollFds[i].fd);
                        i--;
                    } else {
                        throw std::runtime_error("SOME TMP ERROR");
                    }
                    pollFds[i].revents = 0;
                }
            }

		}
	}
}


void Server::listenPort() const {

    if (listen(socketFd, SOMAXCONN) == -1) {
        throw std::runtime_error("ERROR! Cannot listen on the socket");
    }
    std::cout << "Server is listening for incoming connections" << std::endl;
}

int Server::acceptConnection() {
    sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    pollfd clientPollFd;

    // accept connection and dd the new client's socket to the pollFds container
    int clientSocket = accept(socketFd, (sockaddr *) (&clientAddress), &clientAddressLength);
    if (clientSocket == -1) {
        throw std::runtime_error("ERROR! Cannot accept the connection");
    }
	int flags = fcntl(socketFd, F_GETFL, 0);
	if (flags == -1) {
		// Handle error
		throw std::runtime_error("Failed to get socket flags");
	}
	if (fcntl(socketFd, F_SETFL, flags | O_NONBLOCK) == -1) {
		// Handle error
		throw std::runtime_error("Failed to set socket to non-blocking mode");
	}
	clientPollFd.fd = clientSocket;
    clientPollFd.events = POLLIN;
    pollFds.push_back(clientPollFd);
    // print information about the accepted connection
    std::cout << "Accepted connection from: " << inet_ntoa(clientAddress.sin_addr) << ":"
              << ntohs(clientAddress.sin_port)
              << " at fd=" << clientSocket << std::endl;
    return clientSocket;
}

// Channel getters

std::string Server::getServerName() {
    return serverName;
}

std::string Server::getNick(int fd) {
    if (clients.find(fd) != clients.end()) {
        std::string nick = clients[fd]->getNickname();
        return nick;
    }
    return "";
}

void Server::addChannel(Channel *channel) {
	_channels.push_back(channel);
}

Channel* Server::findChannel(const std::string &name) {
	for (std::vector<Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

Client &Server::getClient(int fd) {
	if (clients.find(fd) == clients.end()) {
		throw std::runtime_error("ERROR! Cannot find client with fd");
	}
	return (*this->clients[fd]);
}
