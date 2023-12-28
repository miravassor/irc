#include "Server.hpp"
#include "Channel.hpp"

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
		throw std::runtime_error(
				"Socket error: [" + std::string(strerror(errno)) + "]");
	}
	pollfd serverPollFd;
	serverPollFd.fd = socketFd;
	serverPollFd.events = POLLIN;
	serverPollFd.revents = 0;
	pollFds.push_back(serverPollFd);
	// binding socket to the port
	if (bind(this->socketFd, (sockaddr *) (&address), sizeof(address)) == -1) {
		throw std::runtime_error(
				"Bind error: [" + std::string(strerror(errno)) + "]");
	}
	this->start = time(0);
	this->_password = password;
	this->serverName = "42.IRC";
	this->serverVersion = "0.1";
	initCmd();
	listenPort();
	memset(_buffer, 0, 1024);
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
    cmd["TOPIC"] = &Server::processTopic;
	cmd["PART"] = &Server::processPart;
	cmd["MODE"] = &Server::processMode;
	cmd["NAMES"] = &Server::processNames;
	cmd["LIST"] = &Server::processList;
	cmd["PING"] = &Server::processPing;
	// and other commands
}

Server::~Server() {
	// Memory Cleanup
	std::cout << "[Cleaning before exit]" << std::endl;
	for (std::map<int, Client *>::iterator it = clients.begin();
		 it != clients.end(); ++it) {
		delete it->second;
	}
	for (std::vector<Channel *>::iterator it = _channels.begin();
		 it != _channels.end(); ++it) {
		delete *it;
	}
	// Closing sockets
	for (std::vector<pollfd>::iterator it = pollFds.begin();
		 it != pollFds.end(); ++it) {
		close(it->fd);
	}
}

void Server::addClient(int clientSocket) {
	// Create a new Client object and insert it into the clients map
	clients.insert(std::make_pair(clientSocket, new Client(clientSocket)));
}

void Server::removeClient(int clientSocket) {
	// removing from _channels
	for (std::vector<Channel *>::iterator it = _channels.begin();
		 it != _channels.end(); ++it) {
		(*it)->removeMember(clientSocket);
	}
	// removing from pollFds
	for (std::vector<pollfd>::iterator it = pollFds.begin();
		 it != pollFds.end(); ++it) {
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
	// TODO: iterate through clients to check send Queues
	//  and set sockets to POLLOUT
	for (std::map<int, Client *>::iterator it = clients.begin();
		 it != clients.end(); ++it) {
		if (!it->second->sendQueueEmpty()) {
			for (std::vector<pollfd>::iterator it2 = pollFds.begin();
				 it2 != pollFds.end(); ++it2) {
				if (it2->fd == it->first) {
					it2->events = POLLOUT;
					break;
				}
			}
		}
	}
	int countEvents = poll(&pollFds[0], pollFds.size(), 0);
	if (countEvents < 0) {
		throw std::runtime_error(
				"Poll error: [" + std::string(strerror(errno)) + "]");
	}
	for (size_t i = 0; i < pollFds.size(); i++) {
		if (pollFds[i].revents & POLLIN) {
			i = receiveData(i);
		}
		if (pollFds[i].revents & POLLOUT) {
			sendData(i);
		}

	}
}

size_t Server::receiveData(size_t index) {// if index == 0 -> first connection
	if (index == 0) {
		addClient(acceptConnection());
	} else {
		memset(_buffer, 0, 1024);
		int bytesRead = recv(pollFds[index].fd, _buffer, sizeof(_buffer) - 1,
							 0);
		if (bytesRead > 0) {
			_buffer[bytesRead] = 0;
			if (parsBuffer(pollFds[index].fd)) {
		// TODO : handle parsing errors
				(void) 0;
			}
		} else if (bytesRead == 0) {
			removeClient(pollFds[index].fd);
			index--;
		} else {
			// TODO : handle recv errors
			throw std::runtime_error("SOME TMP ERROR");
		}
		resetEvents(index);
	}
	return index;
}

void Server::sendData(size_t index) {
	try {
		Client &c = getClient(pollFds[index].fd);
		while (!c.sendQueueEmpty()) {
			std::string msg = c.popSendQueue();
			const char *dataPtr = msg.c_str();
			ssize_t dataRemaining = msg.length();
			while (dataRemaining > 0) {
				ssize_t n = send(pollFds[index].fd, dataPtr, dataRemaining, 0);
				if (n < 0) {
					if (errno == EWOULDBLOCK || errno == EAGAIN) {
						c.pushSendQueue(msg);
						pollFds[index].events = POLLOUT;
						break;
					} else {
						throw std::runtime_error("Send error");
					}
				}
				else if (n == 0) {
					removeClient(pollFds[index].fd);
					throw std::runtime_error("Connection closed");
				}
				dataPtr += n;
				dataRemaining -= n;
			}
		}
		pollFds[index].events = POLLIN;
	}
	catch (std::exception &e) {
		std::cout << "[ERR] " << e.what() << std::endl;
	}
	resetEvents(index);
}

void Server::resetEvents(size_t index) {
	pollFds[index].revents = 0;
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
	int clientSocket = accept(socketFd, (sockaddr *) (&clientAddress),
							  &clientAddressLength);
	if (clientSocket == -1) {
		throw std::runtime_error(
				"Accept error: [" + std::string(strerror(errno)) + "]");
	}
	// TODO : Not sur if that part is OK with subject restrictions ?
	int flags = fcntl(socketFd, F_GETFL, 0);
	if (flags == -1) {
		// Handle error
		throw std::runtime_error(
				"Fcntl error: [" + std::string(strerror(errno)) + "]");
	}
	if (fcntl(socketFd, F_SETFL, flags | O_NONBLOCK) == -1) {
		// Handle error
		throw std::runtime_error(
				"Fcntl error: [" + std::string(strerror(errno)) + "]");
	}
	clientPollFd.fd = clientSocket;
	clientPollFd.events = POLLIN;
	clientPollFd.revents = 0;
	pollFds.push_back(clientPollFd);
	// print information about the accepted connection
	std::cout << "Accepted connection from: "
			  << inet_ntoa(clientAddress.sin_addr) << ":"
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

std::vector<std::string> Server::getNicknames(std::set<int> fds) {
	std::vector<std::string> nicknames;
	for (std::set<int>::iterator it = fds.begin(); it != fds.end(); ++it) {
		nicknames.push_back(getNick(*it));
	}
	return nicknames;
}

void Server::addChannel(Channel *channel) {
	_channels.push_back(channel);
}

std::vector<Channel*>::iterator Server::findChannelIterator(const std::string &name) {
    std::vector<Channel*>::iterator it = _channels.begin();
    while (it != _channels.end()) {
        if ((*it)->getName() == name) {
            break;
        }
        it++;
    }
    return it;
}

Channel* Server::findChannel(const std::string &name) {
	for (std::vector<Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

std::vector<Channel *> Server::findChannels(std::queue<std::string> names) {
	std::vector<Channel *> channels;
	while (!names.empty()) {
		Channel *channel = findChannel(names.front());
		if (channel) {
			channels.push_back(channel);
		}
		names.pop();
	}
	return channels;
}

Client *Server::findClient(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (it->second->getNickname() == nickname) {
            return it->second;
        }
    }
    return NULL;
}

Client &Server::getClient(int fd) {
	if (clients.find(fd) == clients.end()) {
		throw std::runtime_error("Cannot find client with fd");
	}
	return (*this->clients[fd]);
}

const std::map<int, Client *> &Server::getClients() const {
	return clients;
}

std::set<int> Server::getClientsFds() const {
	std::set<int> fds;
	for (std::map<int, Client *>::const_iterator it = clients.begin(); it != clients.end(); ++it) {
		fds.insert(it->first);
	}
	return fds;
}
