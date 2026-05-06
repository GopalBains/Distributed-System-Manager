#include "../../headers/serverService.hpp"
#include "../../headers/sockets.hpp"

void *handle_connect_server(void *arg)
{
    TCP socket = *(TCP *)arg;
    char connect_msg = '1';
    socket.sendData(&connect_msg, sizeof(connect_msg));
    delete (TCP *)arg; 
    return NULL;
}
