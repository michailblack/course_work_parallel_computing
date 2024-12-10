#include "ThreadPool.h"

void ThreadPool::Create(uint32_t workersCount)
{
    WriteLock _{ m_ObjectLock };

    if (m_IsInitialized || m_IsTerminated)
        return;

    try
    {
        for (uint32_t i = 0; i < workersCount; ++i)
            m_Workers.emplace_back(&ThreadPool::Routine, this);
        m_IsInitialized = (m_Workers.size() == workersCount);
        m_IsTerminated = !m_IsInitialized;
    }
    catch (...)
    {
        m_IsTerminated = true;
        m_IsInitialized = false;

        for (auto& worker : m_Workers)
        {
            if (worker.joinable())
                worker.join();
        }

        m_Workers.clear();

        throw;
    }
}

void ThreadPool::Start()
{
    WriteLock _{ m_ObjectLock };

    if (!m_IsInitialized || m_IsTerminated)
        throw std::runtime_error(
            "Cannot start an uninitialized or terminated ThreadPool.");

    if (!m_IsPaused)
        return;

    m_IsPaused = false;
    m_PauseWaiter.notify_all();
}

void ThreadPool::Pause()
{
    WriteLock _{ m_ObjectLock };

    if (!IsWorkingUnsafe())
        return;

    m_IsPaused = true;
}

void ThreadPool::Stop()
{
    {
        WriteLock _{ m_ObjectLock };

        if (!IsWorkingUnsafe())
            return;

        m_IsInitialized = false;
        m_IsTerminated = true;
        m_IsPaused = false;
    }

    m_PauseWaiter.notify_all();
    m_TaskWaiter.notify_all();

    for (auto& worker : m_Workers)
    {
        if (worker.joinable())
            worker.join();
    }

    m_Workers.clear();
}

void ThreadPool::Shutdown()
{
    {
        WriteLock lock{ m_ObjectLock };

        if (!IsWorkingUnsafe())
            return;

        std::priority_queue<PriorityTask, std::vector<PriorityTask>, TaskComparator>{}.swap(m_Tasks);
    }

    Stop();
}

void ThreadPool::Routine()
{
    while (true)
    {
        std::function<void()> task;

        {
            WriteLock _{ m_ObjectLock };

            m_PauseWaiter.wait(_, [this] { return !m_IsPaused || m_IsTerminated; });

            m_TaskWaiter.wait(_, [this, &task] {
                if (!m_Tasks.empty())
                {
                    task = std::move(m_Tasks.top().second);
                    m_Tasks.pop();
                    return true;
                }

                return m_IsTerminated;
            });

            if (m_IsTerminated && !task)
                return;
        }

        if (task)
        {
            try
            {
                task();
            }
            catch (...)
            {
                throw;
            }
        }
    }
}
