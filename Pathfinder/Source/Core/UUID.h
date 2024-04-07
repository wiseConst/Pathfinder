#ifndef UUID_H
#define UUID_H

#include <xhash>

namespace Pathfinder
{

class UUID
{
  public:
    UUID();
    UUID(const UUID&) = default;
    ~UUID()           = default;

    operator uint64_t() const { return m_UUID; }

  private:
    uint64_t m_UUID = {0};
};

}  // namespace Pathfinder

namespace std
{
template <> struct hash<Pathfinder::UUID>
{
    size_t operator()(const auto& uuid) const { return std::hash<uint64_t>()(uuid); }
};
}  // namespace std

#endif