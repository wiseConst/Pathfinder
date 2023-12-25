#ifndef INHERITANCE_H
#define INHERITANCE_H

namespace Pathfinder
{

class Unmovable
{
  public:
    Unmovable(const Unmovable&&)            = delete;
    Unmovable& operator=(const Unmovable&&) = delete;

  protected:
    constexpr Unmovable()          = default;
    constexpr virtual ~Unmovable() = default;
};

class Uncopyable
{
  public:
    Uncopyable(const Uncopyable&)            = delete;
    Uncopyable& operator=(const Uncopyable&) = delete;

  protected:
    constexpr Uncopyable()          = default;
    constexpr virtual ~Uncopyable() = default;
};

}  // namespace Pathfinder

#endif  // INHERITANCE_H
