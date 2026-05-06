#include "../../headers/serverService.hpp"
#include "../../headers/Status_codes.hpp"
#include "../../headers/sockets.hpp"

void *handle_status_check(void *arg)
{
    TCP socket = *(TCP *)arg;
    if(OPEN_RECEIVE_FILE_CONNECTION)
    {
        socket.sendData(STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION], strlen(STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION]));
    }
    else
    {
        socket.sendData(STATUS_MESSAGES[SUCCESS], strlen(STATUS_MESSAGES[SUCCESS]));
    }
    delete (TCP *)arg;
    return NULL;
}
