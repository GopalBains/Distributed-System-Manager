#ifndef SERVER_SERVICE_H
#define SERVER_SERVICE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using server_command_handler_t = std::function<void(void *)>;

typedef struct
{
    const char *cmd_name;
    server_command_handler_t handler;
} CommandMapServer;

struct server_args
{
    int server_fd;
};

#define IP_SIZE 16

void *server_listener_thread_tcp(void *args);
void *udp_discovery_responder(void *args);

extern char client_ip[IP_SIZE];

void *handle_status_check(void *arg);

void *handle_connect_server(void *arg);

void *handle_share_file_server(void *arg);

extern int OPEN_RECEIVE_FILE_CONNECTION;

void *handle_receive_file_server(void *arg);

void *handle_close_receive_file_server(void *arg);

void *handle_distributive_systems(void *arg);

extern CommandMapServer server_dispatch_table[];

#endif
