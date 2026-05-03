#ifndef THREAD_SAFETY_H
#define THREAD_SAFETY_H

#include <thread>
#include <iostream>
#include <mutex>
#include <condition_variable>

struct SharedData
{
    std::mutex mtx; // The lock
    std::condition_variable cv; // The signaling mechanism (The "Doorbell")
    int flag = 0; // Your status flag
};
// Shared data

extern SharedData mesh_info_1;
extern SharedData mesh_info_2;
extern char shared_outpath[128];
extern int_fast64_t processedBytes;
extern bool isProcessing;


struct commands_args
{
    SharedData *mesh_info;
};

void wait(SharedData &data);

void resume(SharedData &data);


#endif // THREAD_SAFETY_H
