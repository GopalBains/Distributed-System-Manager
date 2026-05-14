#ifdef _WIN32

#include "../../../headers/clientsCommand.hpp"
#include "../../../headers/shareFile.hpp"
#include "../../../headers/threadSafety.hpp"
#include "../../../headers/Status_codes.hpp"

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <atomic>
#include <future>
#include <thread>
#include <string>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <chrono>

off_t *train_offset;
int_fast64_t *train_chunk_size;

char reducerPath[128] =
    "C:\\dsm\\experimental\\main.cpp";

// ============================================================
// UTIL
// ============================================================

void safeCloseHandle(HANDLE &h)
{
    if (h != NULL &&
        h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h);
        h = NULL;
    }
}

// ============================================================
// FIND CONNECTIONS
// ============================================================

int findTotalConnections()
{
    int connections = 0;

    while (ip_list[connections] != NULL)
        connections++;

    return connections;
}

// ============================================================
// FREE MEMORY
// ============================================================

void freeMemory()
{
    for (int i = 0; ip_list[i] != NULL; i++)
    {
        free(ip_list[i]);
        free(ip_status[i]);
    }

    free(ip_list);
    free(ip_status);

    free(train_offset);
    free(train_chunk_size);
}

// ============================================================
// FILE SIZE
// ============================================================

int_fast64_t getFileSize(
    const char *filePath)
{
    HANDLE hFile = CreateFileA(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("Failed to open file\n");
        return -1;
    }

    LARGE_INTEGER size;

    if (!GetFileSizeEx(hFile, &size))
    {
        safeCloseHandle(hFile);

        printf("Failed to get file size\n");

        return -1;
    }

    safeCloseHandle(hFile);

    return size.QuadPart;
}

// ============================================================
// PREAD WINDOWS
// ============================================================

ssize_t pread_windows(
    HANDLE hFile,
    void *buffer,
    size_t size,
    int64_t offset)
{
    OVERLAPPED ov{};

    ov.Offset =
        static_cast<DWORD>(
            offset & 0xFFFFFFFF);

    ov.OffsetHigh =
        static_cast<DWORD>(
            (offset >> 32) & 0xFFFFFFFF);

    DWORD bytesRead = 0;

    BOOL success = ReadFile(
        hFile,
        buffer,
        static_cast<DWORD>(size),
        &bytesRead,
        &ov);

    if (!success)
        return -1;

    return static_cast<ssize_t>(bytesRead);
}

// ============================================================
// SAFE LINE ALIGNMENT
// ============================================================

int_fast64_t adjustToNextLine(
    HANDLE hFile,
    int_fast64_t pos,
    int_fast64_t fileSize)
{
    if (pos >= fileSize)
        return fileSize;

    const size_t BUF_SIZE = 4096;

    char buffer[BUF_SIZE];

    int_fast64_t start = pos;

    while (pos < fileSize &&
           pos - start < BUF_SIZE * 4)
    {
        ssize_t bytesRead =
            pread_windows(
                hFile,
                buffer,
                BUF_SIZE,
                pos);

        if (bytesRead <= 0)
            break;

        for (ssize_t i = 0;
             i < bytesRead;
             i++)
        {
            if (buffer[i] == '\n')
                return pos + i + 1;
        }

        pos += bytesRead;
    }

    return start;
}

// ============================================================
// DIVIDE FILE INTO CHUNKS
// ============================================================

void divideFileIntoChunks(
    const char *trainfilepath,
    int totalConnections)
{
    HANDLE hFile = CreateFileA(
        trainfilepath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("File open failed\n");
        return;
    }

    int_fast64_t fileSize =
        getFileSize(trainfilepath);

    if (fileSize <= 0)
    {
        printf("Invalid file size\n");

        safeCloseHandle(hFile);

        return;
    }

    int totalWeight = 0;

    for (int i = 0;
         i < totalConnections;
         i++)
    {
        totalWeight += (i == 0) ? 3 : 1;
    }

    int_fast64_t offset = 0;

    for (int i = 0;
         i < totalConnections;
         i++)
    {
        int currWeight =
            (i == 0) ? 3 : 1;

        int_fast64_t ideal_chunk =
            (fileSize * currWeight) /
            totalWeight;

        int_fast64_t tentative_end =
            offset + ideal_chunk;

        int_fast64_t adjusted_end;

        if (i == totalConnections - 1)
        {
            adjusted_end = fileSize;
        }
        else
        {
            adjusted_end =
                adjustToNextLine(
                    hFile,
                    tentative_end,
                    fileSize);

            if (adjusted_end <= offset ||
                adjusted_end >
                    tentative_end +
                        ideal_chunk)
            {
                adjusted_end =
                    tentative_end;
            }
        }

        int_fast64_t final_chunk =
            adjusted_end - offset;

        train_offset[i] = offset;

        train_chunk_size[i] =
            final_chunk;

        offset = adjusted_end;
    }

    safeCloseHandle(hFile);
}

// ============================================================
// SEND FILE THREAD
// ============================================================

void *send_file_thread(
    send_file_args fargs)
{
    int result;

    if (fargs.use_chunk)
    {
        result = send_file(
            fargs.filePath,
            fargs.IP,
            fargs.tag,
            fargs.iscmdSendFile,
            fargs.offset,
            fargs.chunk_size);
    }
    else
    {
        result = send_file(
            fargs.filePath,
            fargs.IP,
            fargs.tag,
            fargs.iscmdSendFile);
    }

    if (result < 0)
    {
        fprintf(stderr,
                "Failed to send %s file to %s\n",
                fargs.tag,
                fargs.IP);
    }

    return NULL;
}

// ============================================================
// COMPILE CODE
// ============================================================

bool compileBinary(
    const std::string &command)
{
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    BOOL success = CreateProcessA(
        NULL,
        const_cast<char *>(
            command.c_str()),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi);

    if (!success)
        return false;

    WaitForSingleObject(
        pi.hProcess,
        INFINITE);

    DWORD exitCode = 0;

    GetExitCodeProcess(
        pi.hProcess,
        &exitCode);

    safeCloseHandle(pi.hProcess);
    safeCloseHandle(pi.hThread);

    return exitCode == 0;
}

// ============================================================
// CREATE CHILD PROCESS
// ============================================================

bool createChildProcess(
    const char *binaryPath,
    HANDLE stdinRead,
    HANDLE stdoutWrite,
    PROCESS_INFORMATION &pi)
{
    STARTUPINFOA si{};

    si.cb = sizeof(si);

    si.dwFlags =
        STARTF_USESTDHANDLES;

    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stdoutWrite;

    BOOL success = CreateProcessA(
        binaryPath,
        NULL,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi);

    return success;
}

// ============================================================
// LOCAL DISTRIBUTIVE COMPUTING
// ============================================================

long long distributiveComputingLocal(
    void *args)
{
    auto *dis_args =
        (distributiveComputingargs *)args;

    printf("Local -> Offset: %lld, Size: %lld\n",
           dis_args->train_offset,
           dis_args->train_chunk_size);

    static const char *binaryPath =
        "C:\\Temp\\local_exec.exe";

    // --------------------------------------------------------
    // COMPILE ONCE
    // --------------------------------------------------------

    static std::once_flag compile_flag;

    std::call_once(
        compile_flag,
        [&]()
        {
            std::string command =
                "g++ " +
                std::string(
                    dis_args->codePath) +
                " -O2 -o " +
                std::string(binaryPath);

            if (!compileBinary(command))
            {
                printf("Compilation failed\n");
                exit(1);
            }
        });

    // --------------------------------------------------------
    // PIPES
    // --------------------------------------------------------

    HANDLE childInputRead = NULL;
    HANDLE childInputWrite = NULL;

    HANDLE childOutputRead = NULL;
    HANDLE childOutputWrite = NULL;

    SECURITY_ATTRIBUTES sa{};

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(
            &childInputRead,
            &childInputWrite,
            &sa,
            0))
    {
        printf("stdin pipe failed\n");
        return -1;
    }

    if (!CreatePipe(
            &childOutputRead,
            &childOutputWrite,
            &sa,
            0))
    {
        printf("stdout pipe failed\n");

        safeCloseHandle(
            childInputRead);

        safeCloseHandle(
            childInputWrite);

        return -1;
    }

    SetHandleInformation(
        childInputWrite,
        HANDLE_FLAG_INHERIT,
        0);

    SetHandleInformation(
        childOutputRead,
        HANDLE_FLAG_INHERIT,
        0);

    // --------------------------------------------------------
    // CREATE PROCESS
    // --------------------------------------------------------

    PROCESS_INFORMATION pi{};

    if (!createChildProcess(
            binaryPath,
            childInputRead,
            childOutputWrite,
            pi))
    {
        printf(
            "Failed to create child process\n");

        return -1;
    }

    safeCloseHandle(
        childInputRead);

    safeCloseHandle(
        childOutputWrite);

    // --------------------------------------------------------
    // OPEN TRAIN FILE
    // --------------------------------------------------------

    HANDLE hFile = CreateFileA(
        dis_args->trainFilePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf(
            "Failed to open train file\n");

        return -1;
    }

    // --------------------------------------------------------
    // SEND DATA TO CHILD
    // --------------------------------------------------------

    const size_t BUF_SIZE = 65536;

    char buffer[BUF_SIZE];

    int64_t remaining =
        dis_args->train_chunk_size;

    int64_t offset =
        dis_args->train_offset;

    while (remaining > 0)
    {
        ssize_t to_read =
            std::min<int64_t>(
                BUF_SIZE,
                remaining);

        ssize_t bytesRead =
            pread_windows(
                hFile,
                buffer,
                to_read,
                offset);

        if (bytesRead <= 0)
        {
            printf("Read failed\n");
            break;
        }

        DWORD totalWritten = 0;

        while (totalWritten <
               static_cast<DWORD>(
                   bytesRead))
        {
            DWORD written = 0;

            BOOL success =
                WriteFile(
                    childInputWrite,
                    buffer +
                        totalWritten,
                    static_cast<DWORD>(
                        bytesRead) -
                        totalWritten,
                    &written,
                    NULL);

            if (!success ||
                written == 0)
            {
                printf(
                    "Write failed\n");

                return -1;
            }

            totalWritten += written;
        }

        offset += bytesRead;
        remaining -= bytesRead;
    }

    safeCloseHandle(hFile);

    safeCloseHandle(
        childInputWrite);

    // --------------------------------------------------------
    // READ OUTPUT
    // --------------------------------------------------------

    std::string output;

    char outbuf[256];

    while (true)
    {
        DWORD bytesRead = 0;

        BOOL success =
            ReadFile(
                childOutputRead,
                outbuf,
                sizeof(outbuf),
                &bytesRead,
                NULL);

        if (!success ||
            bytesRead == 0)
            break;

        output.append(
            outbuf,
            bytesRead);
    }

    safeCloseHandle(
        childOutputRead);

    // --------------------------------------------------------
    // WAIT FOR PROCESS
    // --------------------------------------------------------

    WaitForSingleObject(
        pi.hProcess,
        INFINITE);

    DWORD exitCode = 0;

    GetExitCodeProcess(
        pi.hProcess,
        &exitCode);

    safeCloseHandle(
        pi.hProcess);

    safeCloseHandle(
        pi.hThread);

    if (exitCode != 0)
    {
        printf(
            "Child process failed\n");

        return -1;
    }

    // --------------------------------------------------------
    // PARSE OUTPUT
    // --------------------------------------------------------

    long long result = 0;

    try
    {
        result =
            std::stoll(output);
    }
    catch (...)
    {
        printf(
            "Invalid output\n");

        return -1;
    }

    printf(
        "Local computation result: %lld\n",
        result);

    return result;
}

// ============================================================
// NETWORK DISTRIBUTIVE COMPUTING
// ============================================================

long long distributiveComputing(
    void *args)
{
    auto *dis_args =
        static_cast<
            distributiveComputingargs *>(
            args);

    char message[32] =
        "distributiveComputing";

    TCP socket;

    if (!socket.connect_socket(
            dis_args->IP))
    {
        printf(
            "Failed to connect to IP: %s\n",
            dis_args->IP);

        return -1;
    }

    socket.sendData(
        message,
        strlen(message));

    char res[256];

    int valread =
        socket.receive(
            res,
            sizeof(res) - 1);

    if (valread <= 0)
    {
        printf(
            "Failed to receive response from IP: %s\n",
            dis_args->IP);

        socket.close();

        return -1;
    }

    res[valread] = '\0';

    if (strcmp(
            res,
            STATUS_MESSAGES
                [OPEN_SHAREFILE_CONNECTION]) != 0)
    {
        printf(
            "Connection failed for IP: %s\n",
            dis_args->IP);

        return -1;
    }

    printf(
        "Network -> Offset: %lld, Size: %lld\n",
        dis_args->train_offset,
        dis_args->train_chunk_size);

    const bool iscmdSendFile = false;

    send_file_args code_args{
        dis_args->codePath,
        dis_args->IP,
        "code",
        iscmdSendFile};

    send_file_args reducer_args{
        reducerPath,
        dis_args->IP,
        "reducer",
        iscmdSendFile};

    send_file_args train_args{
        dis_args->trainFilePath,
        dis_args->IP,
        "train",
        iscmdSendFile,
        dis_args->train_offset,
        dis_args->train_chunk_size,
        true};

    std::thread t1(
        send_file_thread,
        code_args);

    std::thread t2(
        send_file_thread,
        reducer_args);

    std::thread t3(
        send_file_thread,
        train_args);

    t1.join();
    t2.join();
    t3.join();

    valread =
        socket.receive(
            res,
            sizeof(res) - 1);

    if (valread <= 0)
    {
        printf(
            "Failed to receive final result from IP: %s\n",
            dis_args->IP);

        socket.close();

        return -1;
    }

    res[valread] = '\0';

    printf(
        "Received result from %s: %s\n",
        dis_args->IP,
        res);

    long long result =
        std::stoll(res);

    return result;
}

// ============================================================
// MAIN HANDLER
// ============================================================

void handle_distributive_systems()
{
    char codepath[128];
    char trainfilepath[128];

    printf("Enter code path: ");

    fgets(
        codepath,
        sizeof(codepath),
        stdin);

    codepath[
        strcspn(codepath, "\n")] = 0;

    printf("Enter train file path: ");

    fgets(
        trainfilepath,
        sizeof(trainfilepath),
        stdin);

    trainfilepath[
        strcspn(trainfilepath, "\n")] = 0;

    // --------------------------------------------------------
    // TIMER START
    // --------------------------------------------------------

    auto start =
        std::chrono::
            high_resolution_clock::now();

    sendRequest();

    int totalConnections =
        findTotalConnections();

    train_offset =
        (off_t *)malloc(
            sizeof(off_t) *
            totalConnections);

    train_chunk_size =
        (int_fast64_t *)malloc(
            sizeof(int_fast64_t) *
            totalConnections);

    divideFileIntoChunks(
        trainfilepath,
        totalConnections);

    // --------------------------------------------------------
    // DEBUG
    // --------------------------------------------------------

    for (int i = 0;
         i < totalConnections;
         i++)
    {
        printf(
            "IP: %s | Offset: %lld | Chunk: %lld\n",
            ip_list[i],
            train_offset[i],
            train_chunk_size[i]);
    }

    // --------------------------------------------------------
    // FUTURES
    // --------------------------------------------------------

    std::future<long long>
        futures[64];

    for (int i = 0;
         i < totalConnections;
         i++)
    {
        auto *dis_args =
            (distributiveComputingargs *)
                malloc(
                    sizeof(
                        distributiveComputingargs));

        dis_args->codePath =
            codepath;

        dis_args->trainFilePath =
            trainfilepath;

        dis_args->IP =
            ip_list[i];

        dis_args->train_offset =
            train_offset[i];

        dis_args->train_chunk_size =
            train_chunk_size[i];

        if (i == 0)
        {
            futures[i] =
                std::async(
                    std::launch::async,
                    distributiveComputingLocal,
                    dis_args);
        }
        else
        {
            futures[i] =
                std::async(
                    std::launch::async,
                    distributiveComputingOverNetwork,
                    dis_args);
        }
    }

    // --------------------------------------------------------
    // COLLECT RESULTS
    // --------------------------------------------------------

    int result = 0;

    for (int i = 0;
         i < totalConnections;
         i++)
    {
        result ^= futures[i].get();
    }

    // --------------------------------------------------------
    // TIMER END
    // --------------------------------------------------------

    auto end =
        std::chrono::
            high_resolution_clock::now();

    auto duration =
        std::chrono::
            duration_cast<
                std::chrono::milliseconds>(
                end - start);

    std::cout
        << "Execution Time: "
        << duration.count() / 1000
        << " sec "
        << duration.count() % 1000
        << " ms\n";

    printf(
        "Final Result: %d\n",
        result);

    freeMemory();
}

#endif
