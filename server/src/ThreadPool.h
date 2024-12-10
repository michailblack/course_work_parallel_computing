#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool
{
public:
    using ReadWriteLock = std::shared_mutex;
    using ReadLock = std::shared_lock<ReadWriteLock>;
    using WriteLock = std::unique_lock<ReadWriteLock>;

public:
    ThreadPool() noexcept = default;
    ~ThreadPool() { Stop(); }

    ThreadPool(const ThreadPool&) noexcept = delete;
    ThreadPool(ThreadPool&&) noexcept = delete;

    ThreadPool& operator=(const ThreadPool&) noexcept = delete;
    ThreadPool& operator=(ThreadPool&&) noexcept = delete;

public:
    void Create(uint32_t workersCount = std::thread::hardware_concurrency() - 1u);

    void Start();
    void Pause();
    void Stop();
    void Shutdown();

    template <typename F, typename... Args>
    auto AddTask(uint8_t priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    bool IsWorking() const
    {
        ReadLock _{ m_ObjectLock };
        return IsWorkingUnsafe();
    }
    bool IsWorkingUnsafe() const noexcept
    {
        return m_IsInitialized && !m_IsTerminated && !m_IsPaused;
    }

private:
    using PriorityTask = std::pair<uint8_t, std::function<void()>>;

private:
    struct TaskComparator
    {
        bool operator()(const PriorityTask& lhs, const PriorityTask& rhs) const noexcept
        {
            return lhs.first > rhs.first;
        }
    };

private:
    void Routine();

private:
    mutable ReadWriteLock               m_ObjectLock{};
    mutable std::condition_variable_any m_TaskWaiter{};
    mutable std::condition_variable_any m_PauseWaiter{};

    std::vector<std::thread> m_Workers{};

    std::priority_queue<PriorityTask, std::vector<PriorityTask>, TaskComparator> m_Tasks{};

    bool m_IsInitialized{ false };
    bool m_IsPaused{ true };
    bool m_IsTerminated{ false };
};

template <typename F, typename... Args>
inline auto ThreadPool::AddTask(uint8_t priority, F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using returnType = typename std::invoke_result<F, Args...>::type;

    auto task{ std::make_shared<std::packaged_task<returnType()>>(
        [func = std::forward<F>(f),
            ... args = std::forward<Args>(args)]() mutable {
            return func(args...);
        }) };

    std::future<returnType> result{ task->get_future() };

    {
        WriteLock lock{ m_ObjectLock };

        if (!IsWorkingUnsafe())
            throw std::runtime_error("ThreadPool is not accepting tasks.");

        m_Tasks.emplace(std::make_pair(priority, [task]() { (*task)(); }));
    }

    m_TaskWaiter.notify_one();
    return result;
}
