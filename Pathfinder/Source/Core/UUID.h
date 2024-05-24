#pragma once

#include <functional>

namespace Pathfinder
{

// TODO: 128 bit handles

class UUID
{
public:
    UUID();
    UUID(const UUID&) = default;
    ~UUID()           = default;
    UUID(const uint64_t uuid);

    operator uint64_t() const { return m_UUID; }

private:
    uint64_t m_UUID = {0};
};

} // namespace Pathfinder

namespace std
{
template <> struct hash<Pathfinder::UUID>
{
    size_t operator()(const auto& uuid) const { return std::hash<uint64_t>()(uuid); }
};
} // namespace std