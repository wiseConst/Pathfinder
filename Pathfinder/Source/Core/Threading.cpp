#include "PathfinderPCH.h"
#include "Threading.h"

namespace Pathfinder
{

static const auto s_MainThreadID = std::this_thread::get_id();

void JobSystem::Init()
{
    s_Workers.resize(GetNumThreads());
    std::ranges::for_each(s_Workers,
                          [&](auto& worker)
                          {
                              worker =
                                  std::jthread{[&]()
                                               {
                                                   if (s_MainThreadID == std::this_thread::get_id()) return;

                                                   while (true)
                                                   {
                                                       Job job;
                                                       {
                                                           std::unique_lock<std::mutex> lock(s_QueueMutex);
                                                           s_Cv.wait(lock, [&] { return !s_JobQueue.empty() || s_bShutdownRequested; });
                                                           if (s_bShutdownRequested && s_JobQueue.empty()) return;

                                                           job = std::move(s_JobQueue.front());
                                                           s_JobQueue.pop();
                                                       }
                                                       job();
                                                   }
                                               }};
                          });

    LOG_TAG_TRACE(JOB_SYSTEM, "JobSystem created!");
}

void JobSystem::Shutdown()
{
    {
        std::unique_lock lock(s_QueueMutex);
        s_bShutdownRequested = true;
    }
    s_Cv.notify_all();
    s_Workers.clear();
    LOG_TAG_TRACE(JOB_SYSTEM, "JobSystem destroyed!");
}

}  // namespace Pathfinder