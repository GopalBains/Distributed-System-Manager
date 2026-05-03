#ifdef __linux__

#include "../../headers/sockets.hpp"
#include "../../headers/clientsCommand.hpp"
#include "../../headers/serverService.hpp"
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sys/sendfile.h>

char client_ip[INET_ADDRSTRLEN] = {0};

Socket::Socket(socket_t fd)
{
    sockfd = fd;
    if (sockfd < 0)
    {
        perror("socket");
    }
}

Socket::Socket()
{
    sockfd = -1;
}

void Socket::setIpFromSockaddr(struct sockaddr_in *address)
{
    inet_ntop(AF_INET, &address->sin_addr, client_ip, INET_ADDRSTRLEN);
}

int Socket::acceptConnection(int server_fd)
{
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    sockfd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (sockfd < 0)
    {
        perror("accept");
        return -1;
    }
    setIpFromSockaddr(&address);
    return 1;
}

int Socket::setupSocket()
{
    struct sockaddr_in address;

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        return -1;
    }

    if (listen(sockfd, 10) < 0)
    {
        perror("listen");
        return -1;
    }
    return sockfd;
}

bool Socket::connect_socket(const char *ip)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        return false;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        return false;
    }

    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);

    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &len) == -1)
    {
        perror("getsockname");
        return false;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, sizeof(ip_str));

    return true;
}

int Socket::sendData(const char *data, size_t len)
{
    return send(sockfd, data, len, 0);
}

int Socket::receive(char *buffer, size_t len)
{
    return recv(sockfd, buffer, len, 0);
}

int Socket::sendFile(int filefd, off_t *offset, size_t chunk)
{
    int sent = sendfile(sockfd, filefd, offset, chunk);
    return sent;
}

void Socket::close()
{
    ::close(sockfd);
}

Socket::~Socket()
{
    close();
}

TCP::TCP() : Socket()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
    }
}

UDP::UDP() : Socket()
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
    }
}

#endif
