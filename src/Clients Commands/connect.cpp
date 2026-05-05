#include "../../headers/clientsCommand.hpp"
#include "../../headers/sockets.hpp"

void handle_connect_client()
{

    int INPUT_SIZE = 100;
    char ip_input[INPUT_SIZE];

    printf("Enter the IP address of the server to connect: ");

    fflush(stdout);

    if (fgets(ip_input, sizeof(ip_input), stdin) != NULL)
    {
        ip_input[strcspn(ip_input, "\n")] = 0; // Remove the newline
    }

    TCP socket;
    if (!socket.connect_socket(ip_input))
    {
        printf("Failed to connect to server at %s\n", ip_input);
        return;
    }

    // TODO:  handle, the logic to ask the I.P that the user wants to connect with, if it wants to connect with my I.P.
    // if yes , connect otherwise don't show.

    // 2. Keep it open for multiple commands
    while (1)
    {
        printf(ANSI_COLOR_GREEN "\nDevices/%s > " ANSI_COLOR_RESET, ip_input);
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0)
            break;

        // Send command through the persistent socket
        if (socket.sendData(command, strlen(command)) < 0)
        {
            printf("Failed to send command.\n");
            break;
        }

        // Receive response
        int valread = socket.receive(buffer, sizeof(buffer) - 1);
        if (valread > 0)
        {
            buffer[valread] = '\0';
            printf("Server: %s\n", buffer);
        }
        else
        {
            printf("Connection lost.\n");
            break;
        }
    }
}
