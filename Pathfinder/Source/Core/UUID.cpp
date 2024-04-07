#include "PathfinderPCH.h"
#include "UUID.h"

namespace Pathfinder
{

static std::random_device s_RandomDevice = {};
static std::mt19937_64 s_Engine(s_RandomDevice());  // Use Mersenne Twister engine
static std::uniform_int_distribution<uint64_t> s_Distribution;

UUID::UUID() : m_UUID(s_Distribution(s_Engine)) {}

}  // namespace Pathfinder
