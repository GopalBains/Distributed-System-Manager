#include "../headers/connection.hpp"
#include "../headers/clientsCommand.hpp"
#include "../headers/serverService.hpp"
#include "../headers/threadSafety.hpp"
#include "../headers/sockets.hpp"

void connection()
{
    TCP socket;
    int server_fd = socket.setupSocket();
    if (server_fd < 0)
    {
        printf("Socket Error");
        exit(EXIT_FAILURE);
    }

    // 2. Thread Management (std::thread)

    // UDP thread
    std::thread server_udp_thread(udp_discovery_responder, nullptr);

    // Server thread
    struct server_args s_args;
    s_args.server_fd = server_fd;

    std::thread server_thread(server_listener_thread_tcp, &s_args);

    // UI thread
     
    

    std::thread ui_thread(commands, nullptr);

    // 3. Join threads
    server_thread.join();
    ui_thread.join();
    server_udp_thread.join();

    close(server_fd);
}
