#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool
{
public:
    explicit ThreadPool(size_t i_threads = std::jthread::hardware_concurrency());
    ~ThreadPool();

    void enqueue(std::function<void()> i_task);

private:
    std::vector<std::jthread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queue_mutex;
    std::condition_variable_any m_condition;
};
