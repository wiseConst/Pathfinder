#pragma once

namespace Pathfinder
{

class Unmovable
{
  public:
    Unmovable(Unmovable&&)            = delete;
    Unmovable& operator=(Unmovable&&) = delete;

  protected:
    constexpr Unmovable()          = default;
};

class Uncopyable
{
  public:
    Uncopyable(const Uncopyable&)            = delete;
    Uncopyable& operator=(const Uncopyable&) = delete;

  protected:
    constexpr Uncopyable()          = default;
};

}  // namespace Pathfinder

