#include "PathfinderPCH.h"
#include "Threading.h"

#if PFR_WINDOWS
#include <windows.h>
#elif PFR_MACOS
#include <pthread.h>
#elif PFR_LINUX
#include <pthread.h>
#else
#error Unknown platform!
#endif

namespace Pathfinder
{

static const auto s_MainThreadID = std::this_thread::get_id();

static void SetThreadName(std::jthread& thread, const std::string& name)
{
#if PFR_WINDOWS

#if _MSC_VER
    std::wstringstream wss;
    wss << name.data() << thread.get_id();

    const HRESULT hr = SetThreadDescription(thread.native_handle(), wss.str().c_str());
    PFR_ASSERT(SUCCEEDED(hr), "Failed to set thread description!");
#endif

#elif PFR_MACOS
    pthread_setname_np(thread.native_handle(), name.c_str());
#elif PFR_LINUX
    pthread_setname_np(thread.native_handle(), name.c_str());
#else
#error Unknown platform!
#endif
}

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

                              const std::string workerDebugName = "JobSystem_Worker_";
                              SetThreadName(worker, workerDebugName);
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