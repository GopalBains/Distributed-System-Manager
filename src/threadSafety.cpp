#include "../headers/threadSafety.hpp"

SharedData mesh_info_1;
SharedData mesh_info_2;
char shared_outpath[128] = {0};
int_fast64_t processedBytes = 0;
bool isProcessing = true;
int_fast64_t Shared_fileSize = 0;

void wait(SharedData &data)
{
    std::unique_lock<std::mutex> lock(data.mtx);

    data.cv.wait(lock, [&]
                 { return data.flag == 1; });

    data.flag = 0;
}

void resume(SharedData &data)
{
    {
        std::lock_guard<std::mutex> lock(data.mtx);
        data.flag = 1;
    }
    data.cv.notify_one();
}
