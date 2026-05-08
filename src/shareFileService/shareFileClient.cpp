#include "../../headers/shareFile.hpp"
#include "../../headers/Status_codes.hpp"
#include "../../headers/clientsCommand.hpp"
#include "../../headers/sockets.hpp"
#include "../../headers/threadSafety.hpp"

#define QUEUE_DEPTH 64
#define BLOCK_SIZE_ 65536

int send_file_zero_copy(TCP *socket, int filefd, off_t start, int_fast64_t end,
                        bool iscmdSendFile, TransferStats &stats, ProgressUI &ui)
{
    off_t offset = start;
    ssize_t sent;

    while (offset < end)
    {
        size_t remaining = end - offset;

        // Limit chunk size (important for stability)
        size_t chunk = remaining > 1 << 20 ? (1 << 20) : remaining; // 1MB chunks

        sent = socket->sendFile(filefd, &offset, chunk);

        if (sent <= 0)
        {
            perror("sendfile");
            return -1;
        }

        if (iscmdSendFile)
        {
            stats.update(sent);
            ui.render(stats);
        }
    }

    return 1;
}

int send_all_sync(TCP *socket, const void *data, size_t len)
{
    const char *ptr = static_cast<const char *>(data);
    while (len > 0)
    {
        ssize_t sent = socket->sendData(ptr, len);
        if (sent <= 0)
            return false;
        ptr += sent;
        len -= sent;
    }
    return true;
}

int send_file(TCP *socket, const char *filename, const char *IP, const char *folder, const bool iscmdSendFile, off_t offset, int_fast64_t chunk)
{
    int file = open(filename, O_RDONLY);
    if (file < 0)
    {
        perror("open");
        return -1;
    }

    struct stat st{};
    if (fstat(file, &st) < 0)
    {
        perror("fstat");
        close(file);
        return -1;
    }

    int_fast64_t filesize;
    if (chunk == (int_fast64_t)-1)
    {
        filesize = st.st_size;
    }
    else
    {
        filesize = chunk;
    }

    uint32_t namelen = strlen(filename);

    int folderlen = strlen(folder);

    if (!send_all_sync(socket, &folderlen, sizeof(folderlen)) ||
        !send_all_sync(socket, folder, folderlen) ||
        !send_all_sync(socket, &namelen, sizeof(namelen)) ||
        !send_all_sync(socket, filename, namelen) ||
        !send_all_sync(socket, &filesize, sizeof(filesize)))
    {
        std::cerr << "Failed to send file metadata\n";
        close(file);
        return -1;
    }

    TransferStats stats;
    ProgressUI ui;

    if (iscmdSendFile)
        stats.start(filesize);

    if (send_file_zero_copy(socket, file, offset, offset + filesize,
                            iscmdSendFile, stats, ui) < 0)
    {
        std::cerr << "Failed to send file using sendfile\n";
        close(file);
        return -1;
    }

    if (iscmdSendFile)
    {
        ui.done();
    }
    return 1;
}

int send_file(const char *filename, const char *IP, const char *folder, const bool iscmdSendFile, off_t offset, int_fast64_t chunk)
{
    // creating and connecting socket.
    TCP socket;
    socket.connect_socket(IP);

    const char *csend_all_syncommand = "shareFile";

    if (!send_all_sync(&socket, csend_all_syncommand, strlen(csend_all_syncommand)))
    {
        perror("Failed to send command");
        return -1;
    }

    char response[128];
    int valread = socket.receive(response, sizeof(response) - 1);
    if (valread <= 0)
    {
        perror("Failed to receive response");
        return -1;
    }
    response[valread] = '\0';
    if (strcmp(response, STATUS_MESSAGES[SUCCESS]) != 0)
    {
        printf("Server response: %s\n", response);
        return -1;
    }

    if (iscmdSendFile)
    {
        printf("\n\n\t\tWait till the receiver accept the connection request............\n\n\n");
    }

    if (!send_all_sync(&socket, &iscmdSendFile, sizeof(iscmdSendFile)))
    {
        perror("Failed to send");
        return -1;
    }

    if (iscmdSendFile)
    {
        char response[128];
        int valread = socket.receive(response, sizeof(response) - 1);
        if (valread <= 0)
        {
            perror("Failed to receive response");
            return -1;
        }
        response[valread] = '\0';
        if (strcmp(response, STATUS_MESSAGES[SUCCESS]) == 0)
        {
            printf("\t\nClient accepted connection\n");
        }
        else
        {
            printf("\t\nClient rejected connection\n");
            return -1;
        }
    }

    return send_file(&socket, filename, IP, folder, iscmdSendFile, offset, chunk);
}
