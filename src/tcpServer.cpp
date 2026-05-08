#include "../headers/connection.hpp"
#include "../headers/serverService.hpp"
#include "../headers/threadSafety.hpp"
#include "../headers/sockets.hpp"

void *server_listener_thread_tcp(void *args)
{
    struct server_args *s_args = (struct server_args *)args;
    while (1)
    {
        Socket *socket = new Socket(1);
        socket->acceptConnection(s_args->server_fd);

        char buffer[256];
        int bytes_read = socket->receive(buffer, sizeof(buffer) - 1);

        if (bytes_read > 0)
        {
             // Null-terminate the string and clean up newlines
            buffer[bytes_read] = '\0';
            buffer[strcspn(buffer, "\r\n")] = 0;

            int found = 0;

            for (int i = 0; server_dispatch_table[i].cmd_name != NULL; i++)
            {
                if (strcmp(buffer, server_dispatch_table[i].cmd_name) == 0)
                {
                    try
                    {
                        std::thread t(server_dispatch_table[i].handler, socket);
                        t.detach();
                    }
                    catch (...)
                    {
                        perror("thread creation failed");
                        delete socket;
                    }

                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                const char *err = "Unknown Command\n";
                socket->sendData(err, strlen(err));
                delete socket;
            }
        }
        else
        {
            delete socket;
        }
    }
}
