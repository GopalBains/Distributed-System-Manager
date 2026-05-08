#include "../headers/serverService.hpp"

CommandMapServer server_dispatch_table[] = {
    {"status", handle_status_check},
    {"connect", handle_connect_server},
    {"shareFile", handle_share_file_server},
    {"receiveFile", handle_receive_file_server}, 
    {"closeReceiveFile" , handle_close_receive_file_server},
    {"distributiveComputing", handle_distributive_systems},
    {NULL, NULL} // Sentinel value to mark the end
};

int OPEN_RECEIVE_FILE_CONNECTION = 0;
