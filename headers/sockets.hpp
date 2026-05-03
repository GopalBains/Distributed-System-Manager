#pragma once
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#endif

using socket_t =
#if defined(_WIN32) || defined(_WIN64)
    SOCKET;
#else
    int;
#endif

class Socket
{
protected:
    socket_t sockfd;

public:
    Socket();
    Socket(socket_t fd);
    ~Socket();

    bool connect_socket(const char *ip);
    int sendData(const char *data, size_t len);
    int receive(char *buffer, size_t len);
    int sendFile(int filefd, off_t *offset, size_t chunk);
    int acceptConnection(int server_fd);
    void close();
    int setupSocket();

    socket_t getSockfd() const { return sockfd; }

    void setIpFromSockaddr(struct sockaddr_in *addr);
};

class TCP : public Socket
{
public:
    TCP();
    TCP(socket_t fd) : Socket(fd) {}
};

class UDP : public Socket
{

public:
    UDP();
    UDP(socket_t fd) : Socket(fd) {}
};
