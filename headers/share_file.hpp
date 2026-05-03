#pragma once
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <chrono>
#include <iomanip>
#include "sockets.hpp"
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#define write_fd _write
#else
#include <unistd.h>
#define write_fd write
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#define _mkdir(dir) mkdir(dir)
#else
#include <sys/stat.h>
#define _mkdir(dir) mkdir(dir, 0755)
#endif

int send_all_sync(int sock, const void *data, size_t len);

int send_file(const char *filename, const char *IP, const char *folder, const bool iscmdSendFile = 0, off_t offset = 0, int_fast64_t chunk = -1);

int send_file(TCP *socket, const char *filename, const char *IP, const char *folder, const bool iscmdSendFile = 0, off_t offset = 0, int_fast64_t chunk = -1);

int recv_all(int sock, void *buf, size_t len);

void create_dirs(const char *path);

int receive_file(Socket *socket, int askclientShareFile);

struct send_file_args
{
    const char *filePath;
    const char *IP;
    const char *tag;
    bool iscmdSendFile;
    long int offset;
    long int chunk_size;
    bool use_chunk;
};

struct TransferStats
{
    uint64_t total_bytes = 0;
    uint64_t transferred = 0;

    std::chrono::steady_clock::time_point start_time;

    void start(uint64_t total)
    {
        total_bytes = total;
        transferred = 0;
        start_time = std::chrono::steady_clock::now();
    }

    void update(uint64_t bytes)
    {
        transferred += bytes;
    }

    double progress() const
    {
        if (total_bytes == 0)
            return 0;
        return (double)transferred * 100.0 / total_bytes;
    }

    double speed() const
    {
        using namespace std::chrono;
        double seconds = duration_cast<duration<double>>(
                             steady_clock::now() - start_time)
                             .count();

        if (seconds == 0)
            return 0;
        return transferred / seconds; // bytes/sec
    }
};

struct ProgressUI
{
    void render(const TransferStats &stats)
    {
        double percent = stats.progress();
        double speed = stats.speed();

        int barWidth = 40;
        int pos = (percent / 100.0) * barWidth;

        std::cout << "\r[";

        for (int i = 0; i < barWidth; i++)
        {
            if (i < pos)
                std::cout << "=";
            else if (i == pos)
                std::cout << ">";
            else
                std::cout << " ";
        }

        std::cout << "] ";

        std::cout << std::fixed << std::setprecision(2)
                  << percent << "% ";

        // Speed formatting
        if (speed < 1024)
            std::cout << speed << " B/s";
        else if (speed < 1024 * 1024)
            std::cout << speed / 1024 << " KB/s";
        else
            std::cout << speed / (1024 * 1024) << " MB/s";

        std::cout.flush();
    }

    void done()
    {
        std::cout << "\nTransfer complete.\n";
    }
};
