#include "../../headers/clientsCommand.hpp"
#include "../../headers/sockets.hpp"

void sendRequest()
{

    UDP socket;

    struct sockaddr_in broadcast_addr;
    int broadcast_enable = 1;
    if (setsockopt(socket.getSockfd(), SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast_enable, sizeof(broadcast_enable)) < 0)
    {
        perror("Setsockopt (SO_BROADCAST) failed");
        socket.close();
        return;
    }

    // 3. Set a Timeout (So we don't wait forever if no one responds)
    struct timeval timeout;
    timeout.tv_sec = 1;  // 1 second
    timeout.tv_usec = 0; // 0 microseconds
    if (setsockopt(socket.getSockfd(), SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
    {
        perror("Setsockopt (SO_RCVTIMEO) failed");
    }

    // 4. Configure Broadcast Address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    snprintf(command, sizeof(command), "status");

    if (sendto(socket.getSockfd(), command, strlen(command), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0)
    {
        perror("sendto failed");
        socket.close();
        return;
    }

    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    int found_count = 0;

    ip_list = (char **)malloc(MAX_IPS * sizeof(char *));
    ip_status = (char **)malloc(MAX_IPS * sizeof(char *));

    if (!ip_list || !ip_status)
    {
        perror("Memory allocation failed");
        socket.close();
        free(ip_list);
        free(ip_status);
        return;
    }

    int count = 0;

    while (1)
    {
        int valread = recvfrom(socket.getSockfd(), buffer, sizeof(buffer) - 1, 0,
                               (struct sockaddr *)&server_addr, &addr_len);

        if (valread < 0)
            break;

        if (count >= MAX_IPS)
        {
            printf("Max IP limit reached\n");
            break;
        }

        buffer[valread] = '\0';

        char *receivedStatus = (char *)malloc(valread + 1);
        if (!receivedStatus)
            break;

        memcpy(receivedStatus, buffer, valread + 1);

        char *ip = (char *)malloc(IP_SIZE);
        if (!ip)
        {
            free(receivedStatus);
            break;
        }

        strncpy(ip, inet_ntoa(server_addr.sin_addr), IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';

        ip_status[count] = receivedStatus;
        ip_list[count] = ip;
        count++;
    }

    socket.close();

    // NULL terminate safely
    if (count < MAX_IPS)
    {
        ip_list[count] = NULL;
        ip_status[count] = NULL;
    }
}

void handle_list_connections()
{
    sendRequest();

    if (!ip_list)
    {
        printf("Failed to retrieve server IPs.\n");
        return;
    }

    printf("Available servers:\n");
    for (int i = 0; ip_list[i] != NULL; i++)
    {
        printf("%s\n", ip_list[i]);
        free(ip_list[i]);   // Free each IP string after use
        free(ip_status[i]); // Free each status string after use
    }
    free(ip_list);   // Free the list itself
    free(ip_status); // Free the status list itself
}
