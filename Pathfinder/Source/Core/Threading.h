#ifndef THREADING_H
#define THREADING_H

#include "Core.h"

#include <thread>
#include <condition_variable>
#include <future>

#include <vector>
#include <functional>
#include <queue>

namespace Pathfinder
{

class JobSystem final : private Uncopyable, private Unmovable
{
  public:
    JobSystem()           = default;
    ~JobSystem() override = default;

    using Job = std::packaged_task<void()>;

    template <typename Func, typename... Args>
    static auto Submit(Func&& func, Args&&... args) -> std::shared_future<decltype(func(args...))>
    {
        using ReturnType = decltype(func(args...));
        std::packaged_task<ReturnType()> task(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::shared_future<ReturnType> future = task.get_future();
        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            s_JobQueue.emplace(std::move(task));
        }
        s_Cv.notify_one();
        return future;
    }

    static void Init();
    static void Shutdown();

    FORCEINLINE static uint32_t GetNumThreads() { return std::thread::hardware_concurrency() - 1; }

  private:
    static inline bool s_bShutdownRequested = false;
    static inline std::vector<std::jthread> s_Workers;
    static inline std::condition_variable_any s_Cv;
    static inline std::mutex s_QueueMutex;
    static inline std::queue<Job> s_JobQueue;
};

}  // namespace Pathfinder

#endif