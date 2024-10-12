#pragma once

class WorkerPool {
public:
    explicit WorkerPool( size_t numThreads = std::thread::hardware_concurrency( ) );
    ~WorkerPool( );

    void Work( std::function<void( )> task );

private:
    std::vector<std::thread> m_threads;
    std::queue<std::unique_ptr<std::function<void( )>>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop{ false };
};