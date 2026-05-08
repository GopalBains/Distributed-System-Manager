#include "../headers/connection.hpp"
#include "../headers/serverService.hpp"
#include "../headers/threadSafety.hpp"
#include "../headers/clientsCommand.hpp"
#include "../headers/sockets.hpp"

void *udp_discovery_responder(void *arg)
{
    int sock;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t len = sizeof(cli_addr);

    // 1. Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    // 2. Bind to the same port the scanner is broadcasting to
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    serv_addr.sin_port = htons(8080);

    if (bind(sock, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("UDP Bind failed");
        return NULL;
    }

    

    // It's only active to respond to the status request from the scanner, so we can keep it in an infinite loop

    while (1)
    {
        // 3. Wait for "status" message
        char buffer[256];
        int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&cli_addr, &len);
        if (n > 0)
        {
            buffer[n] = '\0';
            if (strcmp(buffer, "status") == 0)
            {
                // 4. Send a reply back to the scanner's IP
                char reply = '1'; // You can send any status info you want here
                sendto(sock, &reply, sizeof(reply), 0, (struct sockaddr *)&cli_addr, len);
            }
        }
    }
    return NULL;
}
