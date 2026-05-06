#include "../../headers/serverService.hpp"
#include "../../headers/Status_codes.hpp"
#include "../../headers/sockets.hpp"

void *handle_receive_file_server(void *arg)
{
    TCP socket = *(TCP *)arg;
    // Set the flag to indicate that a receive file connection is open
    OPEN_RECEIVE_FILE_CONNECTION = 1;
    // Send a response to the client indicating that the connection is open
    socket.sendData(STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION], strlen(STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION]));
    delete (TCP *)arg;
    return NULL;
}

void *handle_close_receive_file_server(void *arg)
{
    TCP socket = *(TCP *)arg;
    // Set the flag to indicate that a receive file connection is close.
    OPEN_RECEIVE_FILE_CONNECTION = 0;
    // Send a response to the client indicating that the connection is open
    socket.sendData(STATUS_MESSAGES[SUCCESS], strlen(STATUS_MESSAGES[SUCCESS]));
    delete (TCP *)arg;
    return NULL;
}
