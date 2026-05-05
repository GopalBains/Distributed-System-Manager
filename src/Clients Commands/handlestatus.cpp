#include "../../headers/clientsCommand.hpp"

char *askStatus(const char *SERVER_IP)
{
    const char *statuscommand = "status";
    char *response = sendToServer(statuscommand, SELFIP);
    return response;
}

void handleStatus()
{
    char *response = askStatus(SELFIP);
    if (response)
    {
        printf("Server response: %s\n", response);
        free(response);
    }
    else
    {
        printf("Failed to connect to server or server did not respond.\n");
    }
}
