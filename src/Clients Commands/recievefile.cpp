#include "../../headers/clientsCommand.hpp"
#include "../../headers/Status_codes.hpp"

void handle_receive_file()
{
    printf("Press Q to exit.\n");

    char command[30] = "receiveFile";

    char *result = sendToServer(command, SELFIP);

    if (strcmp(result, STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION]) == 0)
    {
        printf("Waiting to receive file from server...\n");

        while (1)
        {
            
        }
    }
    else
    {
        printf("Failed to receive file: %s\n", result);
    }

    clear_stdin();

    free(result);
}
