#ifdef __linux__

#include "../../../headers/clientsCommand.hpp"
#include "../../../headers/shareFile.hpp"
#include "../../../headers/threadSafety.hpp"
#include "../../../headers/Status_codes.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <atomic>
#include <future>

off_t *train_offset;
int_fast64_t *train_chunk_size;

char reducerPath[128] = "/home/vitthal/dsm/experimental/main.cpp";

// ---------------------- UTIL ----------------------

int findTotalConnections()
{
    int connections = 0;
    while (ip_list[connections] != NULL)
        connections++;
    return connections;
}

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

int_fast64_t getFileSize(const char *filePath)
{
    struct stat st;
    if (stat(filePath, &st) == 0)
        return st.st_size;

    perror("Failed to get file size");
    return -1;
}

// ---------------------- SAFE LINE ALIGNMENT ----------------------

int_fast64_t adjustToNextLine(int fd, int_fast64_t pos, int_fast64_t fileSize)
{
    if (pos >= fileSize)
        return fileSize;

    const size_t BUF_SIZE = 4096;
    char buffer[BUF_SIZE];

    int_fast64_t start = pos;

    while (pos < fileSize && pos - start < BUF_SIZE * 4) // LIMIT scanning
    {
        ssize_t bytesRead = pread(fd, buffer, BUF_SIZE, pos);
        if (bytesRead <= 0)
            break;

        for (ssize_t i = 0; i < bytesRead; i++)
        {
            if (buffer[i] == '\n')
                return pos + i + 1;
        }

        pos += bytesRead;
    }

    // fallback (don’t let it eat whole file)
    return start;
}

// ---------------------- CORE LOGIC ----------------------

void divideFileIntoChunks(const char *trainfilepath, int totalConnections)
{
    int fd = open(trainfilepath, O_RDONLY);
    if (fd < 0)
    {
        perror("File open failed");
        return;
    }

    int_fast64_t fileSize = getFileSize(trainfilepath);
    if (fileSize <= 0)
    {
        perror("Invalid file size");
        close(fd);
        return;
    }

    // ✅ Compute total weight correctly
    int totalWeight = 0;
    for (int i = 0; i < totalConnections; i++)
    {
        totalWeight += (i == 0) ? 3 : 1;
    }

    int_fast64_t offset = 0;

    for (int i = 0; i < totalConnections; i++)
    {
        int currWeight = (i == 0) ? 3 : 1;

        int_fast64_t ideal_chunk = (fileSize * currWeight) / totalWeight;
        int_fast64_t tentative_end = offset + ideal_chunk;
        int_fast64_t adjusted_end;

        if (i == totalConnections - 1)
        {
            // last node gets remaining
            adjusted_end = fileSize;
        }
        else
        {
            adjusted_end = adjustToNextLine(fd, tentative_end, fileSize);

            // safeguard: don’t exceed too much
            if (adjusted_end <= offset || adjusted_end > tentative_end + ideal_chunk)
            {
                adjusted_end = tentative_end;
            }
        }

        int_fast64_t final_chunk = adjusted_end - offset;

        train_offset[i] = offset;
        train_chunk_size[i] = final_chunk;

        offset = adjusted_end;
    }

    close(fd);
}

// ---------------------- THREAD HANDLING ----------------------

void *send_file_thread(send_file_args fargs)
{
    int result;

    if (fargs.use_chunk)
    {
        result = send_file(fargs.filePath, fargs.IP, fargs.tag,
                           fargs.iscmdSendFile,
                           fargs.offset,
                           fargs.chunk_size);
    }
    else
    {
        result = send_file(fargs.filePath, fargs.IP, fargs.tag,
                           fargs.iscmdSendFile);
    }

    if (result < 0)
    {
        fprintf(stderr, "Failed to send %s file to %s\n", fargs.tag, fargs.IP);
    }

    return NULL;
}

long long distributiveComputingOverNetwork(void *args)
{
    auto *dis_args = static_cast<distributiveComputingargs *>(args);

    char message[32] = "distributiveComputing";

    TCP socket;

    if (!socket.connect_socket(dis_args->IP))
    {
        printf("Failed to connect to IP: %s\n", dis_args->IP);
        return -1;
    }

    socket.sendData(message, strlen(message));
    char res[256];
    int valread = socket.receive(res, sizeof(res) - 1);
    if (valread <= 0)
    {
        printf("Failed to receive response from IP: %s\n", dis_args->IP);
        socket.close();
        return -1;
    }

    res[valread] = '\0';

    if (strcmp(res, STATUS_MESSAGES[OPEN_SHAREFILE_CONNECTION]) != 0)
    {
        printf("Connection failed for IP: %s\n", dis_args->IP);
        return -1;
    }

    printf("Network → Offset: %ld, Size: %ld\n",
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

    std::thread t1(send_file_thread, code_args);
    std::thread t2(send_file_thread, reducer_args);
    std::thread t3(send_file_thread, train_args);

    t1.join();
    t2.join();
    t3.join();

    valread = socket.receive(res, sizeof(res) - 1);
    if (valread <= 0)
    {
        printf("Failed to receive final result from IP: %s\n", dis_args->IP);
        socket.close();
        return -1;
    }

    res[valread] = '\0';

    printf("Received result from %s: %s\n", dis_args->IP, res);

    long long result = std::stoi(res);

    return result;
}

long long distributiveComputingLocal(void *args)
{
    auto *dis_args = (distributiveComputingargs *)args;

    printf("Local → Offset: %ld, Size: %ld\n",
           dis_args->train_offset,
           dis_args->train_chunk_size);

    static const char *binaryPath = "/tmp/local_exec";

    // ---------------- STEP 1: Compile ONCE ----------------
    static std::once_flag compile_flag;

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
                   dis_args->codePath,
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

    // ---------------- STEP 2: Pipes ----------------
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

    int fd = open(dis_args->trainFilePath, O_RDONLY);
    if (fd < 0)
    {
        perror("file open failed");
        return -1;
    }

    const size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];

    int64_t remaining = dis_args->train_chunk_size;
    int64_t offset = dis_args->train_offset;

    while (remaining > 0)
    {
        ssize_t to_read = std::min((int64_t)BUF_SIZE, remaining);

        ssize_t bytesRead = pread(fd, buffer, to_read, offset);
        if (bytesRead <= 0)
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

    close(fd);
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
    printf("Local computation result: %lld\n", result);
    return result;
}

// ---------------------- MAIN HANDLER ----------------------

void handle_distributive_systems()
{

    char codepath[128], trainfilepath[128];

    printf("Enter code path: ");
    fgets(codepath, sizeof(codepath), stdin);
    codepath[strcspn(codepath, "\n")] = 0;

    printf("Enter train file path: ");
    fgets(trainfilepath, sizeof(trainfilepath), stdin);
    trainfilepath[strcspn(trainfilepath, "\n")] = 0;

    // Start Timer...

    auto start = std::chrono::high_resolution_clock::now();

    sendRequest();

    int totalConnections = findTotalConnections();

    train_offset = (off_t *)malloc(sizeof(off_t) * totalConnections);
    train_chunk_size = (int_fast64_t *)malloc(sizeof(int_fast64_t) * totalConnections);

    divideFileIntoChunks(trainfilepath, totalConnections);

    // Debug output
    for (int i = 0; i < totalConnections; i++)
    {
        printf("IP: %s | Offset: %ld | Chunk: %ld\n",
               ip_list[i], train_offset[i], train_chunk_size[i]);
    }

    std::future<long long> futures[totalConnections];
    long long int final_result[totalConnections];

    for (int i = 0; i < totalConnections; i++)
    {
        auto *dis_args = (distributiveComputingargs *)malloc(sizeof(distributiveComputingargs));

        dis_args->codePath = codepath;
        dis_args->trainFilePath = trainfilepath;
        dis_args->IP = ip_list[i];
        dis_args->train_offset = train_offset[i];
        dis_args->train_chunk_size = train_chunk_size[i];

        if (i == 0)
            futures[i] = std::async(std::launch::async, distributiveComputingLocal, dis_args);
        else
            futures[i] = std::async(std::launch::async, distributiveComputingOverNetwork, dis_args);
    }

    int result = 0;
    for (int i = 0; i < totalConnections; i++)
    {
        result ^= futures[i].get();
    }

    // END TIMER
    auto end = std::chrono::high_resolution_clock::now();

    // duration in milliseconds
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // print seconds + milliseconds
    std::cout << "Execution Time: "
              << duration.count() / 1000 << " sec "
              << duration.count() % 1000 << " ms\n";
    printf("Final Result: %d\n", result);
    freeMemory();
}

#endif
