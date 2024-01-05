#include "Server.hpp"

void Server::processPing(int fd, const std::vector<std::string> &tokens) {
	if (tokens.size() <= 1) {
		serverSendError(fd, "", ERR_NOORIGIN);
	} else if (tokens[1] != serverName) {
		serverSendError(fd, "", ERR_NOSUCHSERVER);
	} else {
		std::string pong = "PONG :42.IRC\r\n";
        serverSendMessage(fd, pong);
	}
}