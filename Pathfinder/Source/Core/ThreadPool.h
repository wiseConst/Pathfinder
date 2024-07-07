#pragma once

#include <Core/Core.h>

#include <thread>
#include <condition_variable>
#include <future>

#include <vector>
#include <functional>
#include <queue>

namespace Pathfinder
{

class ThreadPool final
{
  public:
    template <typename Func, typename... Args>
    NODISCARD FORCEINLINE static auto Submit(Func&& func, Args&&... args) noexcept -> std::shared_future<decltype(func(args...))>
    {
        using ReturnType = decltype(func(args...));
        std::packaged_task<ReturnType()> task(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        auto future = task.get_future();
        {
            std::scoped_lock lock(s_QueueMutex);
            s_JobQueue.emplace_back([movedTask = std::move(task)]() mutable { movedTask(); });
        }
        s_Cv.notify_one();
        return future;
    }

    static void Init() noexcept;
    static void Shutdown() noexcept;

    NODISCARD FORCEINLINE static uint32_t GetNumThreads() noexcept
    {
        return std::clamp(std::thread::hardware_concurrency() - 1u, 1u, (uint32_t)s_WORKER_THREAD_COUNT);
    }

    // NOTE: Maps thread::id to actual worker index.
    NODISCARD FORCEINLINE static uint8_t MapThreadID(const std::thread::id& threadID) noexcept
    {
        if (threadID == s_MainThreadID) return 0;

        for (uint8_t i{}; i < s_Workers.size(); ++i)
            if (s_Workers[i].get_id() == threadID) return i;

        return 0;
    }

    NODISCARD FORCEINLINE static const auto GetMainThreadID() noexcept { return s_MainThreadID; }

  private:
    static inline std::condition_variable_any s_Cv;
    static inline std::mutex s_QueueMutex;
    static inline std::deque<std::move_only_function<void()>> s_JobQueue;
    static inline std::vector<std::jthread> s_Workers;
    static inline std::thread::id s_MainThreadID = std::this_thread::get_id();
    static inline bool s_bShutdownRequested      = false;

    ThreadPool()  = delete;
    ~ThreadPool() = default;
};

}  // namespace Pathfinder
