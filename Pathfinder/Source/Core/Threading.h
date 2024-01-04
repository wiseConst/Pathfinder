#ifndef THREADING_H
#define THREADING_H

#include <thread>

namespace Pathfinder
{

namespace Threading
{

static  uint32_t GetNumThreads()
{
    return std::thread::hardware_concurrency() - 1;
}

}  // namespace Threading

}  // namespace Pathfinder

#endif