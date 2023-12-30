#include "Channel.hpp"

//Channel::Channel(const std::string &name, Server *server) {
//    this->_name = name;
//	this->_password = "";
//    this->_server = server;
//    this->_visibility = "=";
//    this->_topic = "";
//    this->_mode = 0;
//}

Channel::Channel(const std::string &name, std::string &password) {
	_name = name;
	_password = password;
	_topic = "";
	_mode = 0;
	setMode(TOPICSET);
	if (!_password.empty()) {
		setMode(KEYSET);
	}
}

//Channel::Channel(const std::string &name, std::string &password, Server *server) {
//	this->_name = name;
//	this->_password = password;
//    this->_server = server;
//    this->_visibility = "=";
//    this->_topic = "";
//    this->_mode = 0;
//}

Channel::~Channel() {
}


const std::string &Channel::getName() const {
	return _name;
}

const std::string &Channel::getTopic() const {
	return _topic;
}

unsigned int Channel::getMode() const {
	return _mode;
}

std::string Channel::getModeString() const {
	std::string modeString = "+";

	if (isModeSet(INVITEONLY)) {
		modeString += 'i';
	}
	if (isModeSet(TOPICSET)) {
		modeString += 't';
	}
	if (isModeSet(KEYSET)) {
		modeString += 'k';
	}
	if (isModeSet(LIMITSET)) {
		modeString += 'l';
	}
	return modeString;
}

std::string Channel::getModeStringWithParameters() const {
	std::string modeString = getModeString();
	if (isModeSet(KEYSET) && !_password.empty()) {
		modeString += " " + _password;
	}
	if (isModeSet(LIMITSET) && limitMembers > 0) {
		std::ostringstream oss;
		oss << limitMembers;
		modeString += " " + oss.str();
	}
	return modeString;
}

const std::set<int> &Channel::getMemberFds() const {
	return _memberFds;
}

const std::set<int> &Channel::getOperatorFds() const {
	return _operatorFds;
}

int Channel::getLimitMembers() const {
	return limitMembers;
}

void Channel::setTopic(const std::string &topic) {
	_topic = topic;
}

void Channel::setPassword(const std::string &password) {
	_password = password;
}

void Channel::setLimitMembers(int limitMembers) {
	Channel::limitMembers = limitMembers;
}

bool Channel::setMode(unsigned int mode) {
	if (isModeSet(mode)) {
		return false;
	}
	_mode |= mode;
	return true;
}

bool Channel::unsetMode(unsigned int mode) {
	if (!isModeSet(mode)) {
		return false;
	}
	_mode &= ~mode;
	return true;
}

bool Channel::isModeSet(unsigned int mode) const {
	return (_mode & mode) == mode;
}


void Channel::addMember(int clientFd) {
	_memberFds.insert(clientFd);
}

void Channel::removeMember(int clientFd) {
	removeInvited(clientFd);
	removeOperator(clientFd);
	_memberFds.erase(clientFd);
}

bool Channel::hasMember(int clientFd) {
	return _memberFds.find(clientFd) != _memberFds.end();
}

bool Channel::authMember(int clientFd, std::string &password) {
	if (password != _password) {
		return false;
	}
	removeInvited(clientFd);
	addMember(clientFd);
	return true;
}

bool Channel::addOperator(int clientFd) {
	return _operatorFds.insert(clientFd).second;
}


bool Channel::removeOperator(int clientFd) {
	return _operatorFds.erase(clientFd);
}

bool Channel::hasOperator(int clientFd) {
	return _operatorFds.find(clientFd) != _operatorFds.end();
}

void Channel::addInvited(int clientFd) {
	_invitedFds.insert(clientFd);
}

void Channel::removeInvited(int clientFd) {
	_invitedFds.erase(clientFd);
}

bool Channel::hasInvited(int clientFd) {
	return _invitedFds.find(clientFd) != _invitedFds.end();
}


//void    Channel::newMember(int fd) {
//    if (!getTopic().empty()) {
//        chanReply(fd, RPL_TOPIC);
//    }
//    else {
//        chanReply(fd, RPL_NOTOPIC);
//    }
//    chanReply(fd, RPL_NAMREPLY);
//}

//void    Channel::chanReply(int fd, chanRep id) {
//    switch (id) {
//        case RPL_TOPIC:
//		    // to do: replace <topic> with real value
//            // send(fd, replyStr.c_str(), replyStr.length(), 0);
//			// serverSendReply(fd, "332", "<topic>");
//			break;
//		case RPL_NOTOPIC:
//			chanSendReply(fd, "331", _server->getNick(fd) + " " + _name, "No topic is set");
//			break;
//		case RPL_NAMREPLY:
//            std::string nmrp = ":" + _server->getServerName() + " " + _server->getNick(fd) + " 353 " + _visibility + " " + _name + " ";
//            std::set<int>::iterator it = _memberFds.begin();
//            for (; it != _memberFds.end(); ++it) {
//                nmrp.append(_server->getNick(*it));
//                std::set<int>::iterator nextIt = it;
//                ++nextIt;
//                if (nextIt != _memberFds.end())
//                    nmrp.append(" ");
//            }
//			try {
//				nmrp.append("\n");
//				_server->getClient(fd).pushSendQueue(nmrp);
//			} catch (std::exception &e) {
//				std::cerr << e.what() << std::endl;
//			}
////            send(fd, nmrp.c_str(), nmrp.length(), 0);
//			break;
//    }
//}
//
//void    Channel::chanSendReply(int fd, std::string id, const std::string &token, const std::string &reply) {
//    std::stringstream fullReply;
//	fullReply << ":" << _server->getServerName() << " " << id << " " << token << " :" << reply << "\r\n";
//	std::string replyStr = fullReply.str();
//	try {
//		_server->getClient(fd).pushSendQueue(replyStr);
//	} catch (std::exception &e) {
//		std::cout << "[ERR] " << e.what() << std::endl;
//	}
//}

