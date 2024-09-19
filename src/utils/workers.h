#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>

class WorkerPool {
public:
    WorkerPool( size_t num_threads = std::thread::hardware_concurrency( ) );
    ~WorkerPool( );

    void work( std::function<void( )> task );

private:
    std::vector<std::thread> threads;
    std::queue<std::unique_ptr<std::function<void( )>>> tasks;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{ false };
};