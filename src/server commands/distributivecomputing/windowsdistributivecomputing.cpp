#ifdef _WIN32

#include "../../../headers/serverService.hpp"
#include "../../../headers/Status_codes.hpp"
#include "../../../headers/sockets.hpp"
#include "../../../headers/shareFile.hpp"
#include "../../../headers/threadSafety.hpp"
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <atomic>
#include <future>
#include <thread>
#include <string>
#include <iostream>

// ------------------------------------------------------------
// PREAD EMULATION
// ------------------------------------------------------------

ssize_t pread_windows(HANDLE hFile,
                      void *buffer,
                      size_t size,
                      int64_t offset)
{
    OVERLAPPED ov = {};

    ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)((offset >> 32) & 0xFFFFFFFF);

    DWORD bytesRead = 0;

    BOOL ok = ReadFile(
        hFile,
        buffer,
        (DWORD)size,
        &bytesRead,
        &ov);

    if (!ok)
        return -1;

    return bytesRead;
}

// ------------------------------------------------------------
// DISTRIBUTIVE COMPUTING
// ------------------------------------------------------------

long long distributiveComputing(
    int64_t offset,
    int64_t remaining,
    HANDLE hFile)
{
    static const char *binaryPath =
        "C:\\Temp\\local_exec.exe";

    // --------------------------------------------------------
    // CREATE PIPES
    // --------------------------------------------------------

    HANDLE childStdoutRead;
    HANDLE childStdoutWrite;

    HANDLE childStdinRead;
    HANDLE childStdinWrite;

    SECURITY_ATTRIBUTES sa = {};

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(
            &childStdoutRead,
            &childStdoutWrite,
            &sa,
            0))
    {
        printf("stdout pipe failed\n");
        return -1;
    }

    if (!CreatePipe(
            &childStdinRead,
            &childStdinWrite,
            &sa,
            0))
    {
        printf("stdin pipe failed\n");
        return -1;
    }

    SetHandleInformation(
        childStdoutRead,
        HANDLE_FLAG_INHERIT,
        0);

    SetHandleInformation(
        childStdinWrite,
        HANDLE_FLAG_INHERIT,
        0);

    // --------------------------------------------------------
    // START PROCESS
    // --------------------------------------------------------

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};

    si.cb = sizeof(si);

    si.dwFlags |= STARTF_USESTDHANDLES;

    si.hStdInput = childStdinRead;
    si.hStdOutput = childStdoutWrite;
    si.hStdError = childStdoutWrite;

    BOOL ok = CreateProcessA(
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

    if (!ok)
    {
        printf("CreateProcess failed\n");

        CloseHandle(childStdoutRead);
        CloseHandle(childStdoutWrite);
        CloseHandle(childStdinRead);
        CloseHandle(childStdinWrite);

        return -1;
    }

    CloseHandle(childStdoutWrite);
    CloseHandle(childStdinRead);

    // --------------------------------------------------------
    // SEND FILE DATA TO CHILD
    // --------------------------------------------------------

    const size_t BUF_SIZE = 65536;

    char buffer[BUF_SIZE];

    while (remaining > 0)
    {
        ssize_t to_read =
            std::min((int64_t)BUF_SIZE,
                     remaining);

        ssize_t bytesRead =
            pread_windows(
                hFile,
                buffer,
                to_read,
                offset);

        if (bytesRead <= 0)
        {
            printf("pread failed\n");
            break;
        }

        DWORD totalWritten = 0;

        BOOL success =
            WriteFile(
                childStdinWrite,
                buffer,
                bytesRead,
                &totalWritten,
                NULL);

        if (!success)
        {
            printf("write failed\n");

            CloseHandle(childStdinWrite);
            CloseHandle(childStdoutRead);

            return -1;
        }

        offset += bytesRead;
        remaining -= bytesRead;
    }

    CloseHandle(childStdinWrite);

    // --------------------------------------------------------
    // READ CHILD OUTPUT
    // --------------------------------------------------------

    std::string output;

    char buf[256];

    DWORD bytesRead = 0;

    while (true)
    {
        BOOL success =
            ReadFile(
                childStdoutRead,
                buf,
                sizeof(buf),
                &bytesRead,
                NULL);

        if (!success || bytesRead == 0)
            break;

        output.append(buf, bytesRead);
    }

    CloseHandle(childStdoutRead);

    // --------------------------------------------------------
    // WAIT FOR PROCESS
    // --------------------------------------------------------

    WaitForSingleObject(
        pi.hProcess,
        INFINITE);

    DWORD exitCode;

    GetExitCodeProcess(
        pi.hProcess,
        &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0)
    {
        printf("Child process failed\n");
        return -1;
    }

    // --------------------------------------------------------
    // PARSE RESULT
    // --------------------------------------------------------

    long long result = 0;

    try
    {
        result = std::stoll(output);
    }
    catch (...)
    {
        printf("Invalid output: %s\n",
               output.c_str());

        return -1;
    }

    return result;
}

// ------------------------------------------------------------
// COMPILE CODE
// ------------------------------------------------------------

void compileCode()
{
    static const char *binaryPath =
        "C:\\Temp\\local_exec.exe";

    static std::once_flag compile_flag;

    const char *codepath =
        "received_files\\code\\main.cpp";

    std::call_once(
        compile_flag,
        [&]()
        {
            std::string cmd =
                "g++ " +
                std::string(codepath) +
                " -O2 -o " +
                std::string(binaryPath);

            STARTUPINFOA si = {};
            PROCESS_INFORMATION pi = {};

            si.cb = sizeof(si);

            BOOL ok = CreateProcessA(
                NULL,
                cmd.data(),
                NULL,
                NULL,
                FALSE,
                0,
                NULL,
                NULL,
                &si,
                &pi);

            if (!ok)
            {
                printf("Compilation process failed\n");
                exit(1);
            }

            WaitForSingleObject(
                pi.hProcess,
                INFINITE);

            DWORD exitCode;

            GetExitCodeProcess(
                pi.hProcess,
                &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (exitCode != 0)
            {
                printf("Compilation failed\n");
                exit(1);
            }
        });
}

// ------------------------------------------------------------
// HANDLE DISTRIBUTIVE SYSTEMS
// ------------------------------------------------------------

void *handle_distributive_systems(void *arg)
{
    TCP *socket = (TCP *)arg;

    OPEN_RECEIVE_FILE_CONNECTION = 1;

    socket->sendData(
        STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION],
        strlen(
            STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION]));

    int askClientShareFile = 1;

    // Wait for code file
    wait(mesh_info_1);

    compileCode();

    // Wait until some file is received
    wait(mesh_info_2);

    const char *trainfilepath =
        "received_files\\train\\test.txt";

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
        printf("file open failed\n");
        return NULL;
    }

    int final_result = 0;

    int_fast64_t offset = 0;
    int_fast64_t remaining = 0;

    while (true)
    {
        if (offset == Shared_fileSize)
            break;

        remaining = processedBytes - offset;

        int temp =
            distributiveComputing(
                offset,
                remaining,
                hFile);

        final_result ^= temp;

        offset += remaining;
    }

    std::string result =
        std::to_string(final_result);

    socket->sendData(
        result.c_str(),
        result.length());

    OPEN_RECEIVE_FILE_CONNECTION = 0;

    CloseHandle(hFile);

    delete socket;

    return NULL;
}

#endif
