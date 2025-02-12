#ifndef TASKPOOL_H_INCLUDED
#define TASKPOOL_H_INCLUDED

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Util
{

class Task
{
public:
    virtual void operator()() = 0;
};

class TaskPool
{
public:
    typedef std::function<void()> Callback;

    TaskPool(const std::vector<std::shared_ptr<Task>>& queue, Callback dequeued, Callback completion, bool multithread = true);
    TaskPool() = delete;
    ~TaskPool();

    void WaitToComplete();

private:
    std::vector<std::thread> _threads;
    std::vector<std::shared_ptr<Task>> _queue;
    std::mutex _mutex;
    Callback _dequeued;
    Callback _completion;
};

inline TaskPool::TaskPool(const std::vector<std::shared_ptr<Task>>& queue, Callback dequeued, Callback completion, bool multithread)
    : _queue(queue)
    , _dequeued(dequeued)
    , _completion(completion)
{
    const size_t coreCount = std::thread::hardware_concurrency();
    size_t threadCount = multithread ? std::min(coreCount, queue.size()) : 1;

    for (size_t i = 0; i < threadCount; ++i) {
        // Create a thread with a lambda that loops through the queue
        // of file paths and processes them.

        // TODO: start the threads as sleeping and move the population of the queue to another function.
        // In this way, the same set of worker threads can be reused by various parallel jobs.
        _threads.emplace_back([this]{
            while (true) 
            {
                bool finalTask = false;
                std::shared_ptr<Task> task;

                {
                    std::unique_lock<std::mutex> lock(_mutex);

                    if (_queue.empty())
                    {
                        break;
                    }
                    task = _queue.back();
                    _queue.pop_back();

                    finalTask = _queue.empty();
                }

                if (task)
                {
                    (*task)();
                }

                if (_dequeued && finalTask)
                {
                    // Callback to indicate that the final task has been dequeued.
                    // Allowing the next stage of processing to begin.
                    _dequeued();
                }
            }
        });
    }
}

inline TaskPool::~TaskPool()
{
    WaitToComplete();
}

inline void TaskPool::WaitToComplete()
{
    for (auto &thread : _threads) {
        thread.join();
    }
    _threads.clear();

    if (_completion)
    {
        // Callback to indicate that all tasks have completed.
        _completion();
        _completion = nullptr;
    }
}

} // namespace Spock

#endif // TASKPOOL_H_INCLUDED