#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "NativeSocket.h"

int NativeSocket::socket() {
    int socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0) {
        Exception::io("NativeSocket.socket()");
    }
    return socketFd;
}

void NativeSocket::bind(int socketFd, int port) {
    struct sockaddr_in servAddr;

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(port);
    if (::bind(socketFd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        Exception::io("NativeSocket.bind()");
    }
}

void NativeSocket::listen(int listenerSocketFd) {
    ::listen(listenerSocketFd, 5);
}

int NativeSocket::accept(int listenerSocketFd) {
    struct sockaddr_in clientAddr;

    socklen_t clientLen = sizeof(clientAddr);
    int acceptedSocketFd = ::accept(listenerSocketFd,
                                    (struct sockaddr *) &clientAddr,
                                    &clientLen);
    if (acceptedSocketFd < 0) {
        Exception::io("NativeSocket.accept()");
    }
    return acceptedSocketFd;
}

bool NativeSocket::connect(int socketFd, Pointer<string> host, int port) {
    std::string hostName(host->buf->data(), host->buf->length());
    struct hostent* server = gethostbyname(hostName.c_str());
    if (server == nullptr) {
        Exception::io("NativeSocket.connect()");
    }

    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    memcpy(&servAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    servAddr.sin_port = htons(port);
    if (::connect(socketFd,
                  (struct sockaddr *) &servAddr,
                  sizeof(servAddr)) < 0) {
        return false;
    }
    return true;
}
