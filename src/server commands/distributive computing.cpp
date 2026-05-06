#include "../../headers/serverService.hpp"
#include "../../headers/Status_codes.hpp"
#include "../../headers/sockets.hpp"
#include "../../headers/shareFile.hpp"
#include "../../headers/threadSafety.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <atomic>
#include <future>

long long distributiveComputing(int64_t offset, int64_t remaining, int fd)
{
    static const char *binaryPath = "/tmp/local_exec";
    // ---------------- Pipes ----------------
    int inpipe[2];
    int outpipe[2];
    if (pipe(inpipe) == -1 || pipe(outpipe) == -1)
    {
        perror("pipe failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        return -1;
    }

    if (pid == 0)
    {
        // CHILD

        // stdin
        if (dup2(inpipe[0], STDIN_FILENO) == -1)
        {
            perror("dup2 stdin failed");
            _exit(1);
        }

        // stdout
        if (dup2(outpipe[1], STDOUT_FILENO) == -1)
        {
            perror("dup2 stdout failed");
            _exit(1);
        }

        // close everything
        close(inpipe[0]);
        close(inpipe[1]);
        close(outpipe[0]);
        close(outpipe[1]);

        if (access(binaryPath, X_OK) != 0)
        {
            perror("binary not executable");
            _exit(1);
        }
        execl(binaryPath, binaryPath, NULL);

        perror("exec failed");
        _exit(1);
    }

    // ---------------- PARENT ----------------
    close(inpipe[0]);
    close(outpipe[1]);

    const size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];

    while (remaining > 0)
    {
        ssize_t to_read = std::min((int64_t)BUF_SIZE, remaining);
        ssize_t bytesRead = pread(fd, buffer, to_read, offset);

        if (bytesRead < 0)
        {
            perror("pread failed");
            break;
        }

        ssize_t total_written = 0;
        while (total_written < bytesRead)
        {
            ssize_t w = write(inpipe[1],
                              buffer + total_written,
                              bytesRead - total_written);

            if (w <= 0)
            {
                perror("write failed");
                close(fd);
                close(inpipe[1]);
                close(outpipe[0]);
                return -1;
            }
            total_written += w;
        }

        offset += bytesRead;
        remaining -= bytesRead;
    }

    close(inpipe[1]); // EOF to child

    // ---------------- READ OUTPUT ----------------
    std::string output;
    char buf[256];

    while (true)
    {
        ssize_t n = read(outpipe[0], buf, sizeof(buf));

        if (n == 0)
            break;
        if (n < 0)
        {
            perror("read failed");
            break;
        }
        output.append(buf, n);
    }

    close(outpipe[0]);

    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status))
    {
        fprintf(stderr, "Child crashed\n");
        return -1;
    }

    // ---------------- PARSE RESULT ----------------
    long long result = 0;
    try
    {
        result = std::stoll(output);
    }
    catch (...)
    {
        fprintf(stderr, "Invalid output: %s\n", output.c_str());
        return -1;
    }

    return result;
}

void compileCode()
{
    static const char *binaryPath = "/tmp/local_exec";

    static std::once_flag compile_flag;

    const char *codepath = "received_files/code/main.cpp";

    std::call_once(compile_flag, [&]()
                   {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork failed (compile)");
            exit(1);
        }

        if (pid == 0)
        {
            execlp("g++", "g++",
                   codepath,
                   "-O2",
                   "-o", binaryPath,
                   NULL);

            perror("exec g++ failed");
            _exit(1);
        }

        int status;
        if (waitpid(pid, &status, 0) == -1)
        {
            perror("waitpid failed");
            exit(1);
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            fprintf(stderr, "Compilation failed\n");
            exit(1);
        } });
}

void *handle_distributive_systems(void *arg)
{
    TCP *socket = (TCP *)arg;
    OPEN_RECEIVE_FILE_CONNECTION = 1;

    socket->sendData(
        STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION],
        strlen(STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION]));

    int askClientShareFile = 1;

    // Wait for the client to finish sending the code file..

    wait(mesh_info_1);
    compileCode();
    // Waiting until , a small part of the file is received..
    wait(mesh_info_2);
    // Recieved the file, now we can start processing it in chunks and send the result back to client.
    const char *trainfilepath = "received_files/train/test.txt";

    int fd = open(trainfilepath, O_RDONLY);
    if (fd < 0)
    {
        perror("file open failed");
    }

    int final_result = 0;
    int_fast64_t offset = 0;
    int_fast64_t remaining = 0;
    while (true)
    {
        if (offset == Shared_fileSize)
        {
            break;
        }
        remaining = processedBytes - offset;
        int temp = distributiveComputing(offset, remaining, fd);
        final_result ^= temp;
        offset += remaining;
    }

    socket->sendData(std::to_string(final_result).c_str(),
                     std::to_string(final_result).length());
    OPEN_RECEIVE_FILE_CONNECTION = 0;

    close(fd);
    delete socket;
    return NULL;
}
