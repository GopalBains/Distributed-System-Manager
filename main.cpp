#include <stdio.h>
#include "headers/connection.hpp"
#include "headers/sockets.hpp"

int main()
{
#if defined(_WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    connection();

#if defined(_WIN32)
    WSACleanup();
#endif

    return 0;
}
