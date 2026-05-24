#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t i_threads)
{
    m_workers.reserve(i_threads);

    for (size_t i = 0; i < i_threads; ++i)
    {
        // std::jthread automatically joins on destruction
        m_workers.emplace_back([this](std::stop_token i_stop)
            {
                while (!i_stop.stop_requested())
                {
                    std::function<void()> task{};
                    {
                        std::unique_lock lock(m_queue_mutex);
                        // Wait for a task or a stop request
                        m_condition.wait(lock, i_stop, [this]
                            {
                                return !m_tasks.empty();
                            });

                        if (i_stop.stop_requested() && m_tasks.empty()) return;

                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    task(); // Execute the fire-and-forget task
                }
            });
    }
}

ThreadPool::~ThreadPool()
{
    for (std::jthread& worker : m_workers)
    {
        worker.request_stop();
    }
    
    m_condition.notify_all();
    
    for (std::jthread& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

void ThreadPool::enqueue(std::function<void()> i_task)
{
    {
        std::lock_guard lock(m_queue_mutex);
        m_tasks.push(std::move(i_task));
    }
    m_condition.notify_one();
}
