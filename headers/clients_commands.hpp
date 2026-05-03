#ifndef CLIENTS_COMMAND_H
#define CLIENTS_COMMAND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

typedef void (*client_command_handler_t)();

typedef struct
{
    const char *cmd_name;
    client_command_handler_t handler;
} CommandMapClient;

#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define IP_LENGTH INET_ADDRSTRLEN
#define PORT 8080 // Ensure your server is listening on this UDP port
#define BUFFER_SIZE 8192
#define TOTAL_IPS 300
#define IP_SIZE 16

extern char buffer[BUFFER_SIZE];
extern char command[BUFFER_SIZE];

extern const char *SELFIP;
 
const int MAX_IPS = 300;
extern char **ip_list;   // Global variable to store the list of IPs
extern char **ip_status; // Global variable to store the list of IPs that responded to status command

extern CommandMapClient client_dispatch_table[];

struct distributiveComputingargs
{
    char *codePath;
    char *trainFilePath;
    long int train_offset;
    long int train_chunk_size;
    char *IP;
};

char *sendToServer(const char *command, const char *IP);

void handle_exit();

void handle_connect_client();

void clear_stdin();

void handleStatus();

void handle_list_connections();

void handle_distributive_systems();

void sendRequest();

void handle_send_file();

void handle_receive_file();

void *commands(void *args);

#endif
