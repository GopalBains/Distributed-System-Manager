#ifdef _WIN32

#include "../../headers/sockets.hpp"
#include "../../headers/clientsCommand.hpp"
#include "../../headers/serverService.hpp"

#include <iostream>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

char client_ip[INET_ADDRSTRLEN] = {0};

Socket::Socket()
{
    sockfd = -1;
}

Socket::Socket(socket_t fd)
{
    sockfd = fd;
    if (sockfd == INVALID_SOCKET)
    {
        std::cerr << "socket error: " << WSAGetLastError() << std::endl;
    }
}

void Socket::setIpFromSockaddr(struct sockaddr_in *address)
{
    inet_ntop(AF_INET, &(address->sin_addr), client_ip, INET_ADDRSTRLEN);
}

int Socket::acceptConnection(int server_fd)
{
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    sockfd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (sockfd == INVALID_SOCKET)
    {
        std::cerr << "accept error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    setIpFromSockaddr(&address);
    return 1;
}

bool Socket::connect_socket(const char *ip)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
    {
        std::cerr << "inet_pton error\n";
        return false;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "connect error: " << WSAGetLastError() << std::endl;
        return false;
    }
    return true;
}

int Socket::setupSocket()
{
    struct sockaddr_in address;
    int opt = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        return -1;
    }

    if (listen(sockfd, 10) < 0)
    {
        perror("listen");
        return -1;
    }

    return sockfd;
}

int Socket::sendData(const char *data, size_t len)
{
    return send(sockfd, data, (int)len, 0);
}

int Socket::receive(char *buffer, size_t len)
{
    return recv(sockfd, buffer, (int)len, 0);
}

// ❗ Windows doesn't have sendfile → manual implementation
int Socket::sendFile(int filefd, off_t *offset, size_t chunk)
{

    char buffer[BUFFER_SIZE];

    int totalSent = 0;
    int bytesRead;

    // Move file pointer to offset
    _lseek(filefd, *offset, SEEK_SET);

    while ((bytesRead = _read(filefd, buffer, BUFFER_SIZE)) > 0)
    {
        int sent = send(sockfd, buffer, bytesRead, 0);
        if (sent == SOCKET_ERROR)
        {
            std::cerr << "send error: " << WSAGetLastError() << std::endl;
            return -1;
        }
        totalSent += sent;
        *offset += sent;
    }

    return totalSent;
}

void Socket::close()
{
    closesocket(sockfd);
}

Socket::~Socket()
{
    close();
}

TCP::TCP() : Socket()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET)
    {
        std::cerr << "socket error: " << WSAGetLastError() << std::endl;
    }
}

UDP::UDP() : Socket()
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET)
    {
        std::cerr << "socket error: " << WSAGetLastError() << std::endl;
    }
}

#endif
