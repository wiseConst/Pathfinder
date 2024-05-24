#pragma once

#include "Core.h"
#include "Keys.h"

namespace Pathfinder
{

class Input final
{
  public:
    static bool IsKeyPressed(const EKey key);
    static bool IsKeyReleased(const EKey key);
    static bool IsKeyRepeated(const EKey key);

    static bool IsMouseButtonPressed(const EKey button);
    static bool IsMouseButtonReleased(const EKey button);
    static bool IsMouseButtonRepeated(const EKey button);

    static std::pair<int32_t, int32_t> GetMousePosition();

  private:
    Input()  = delete;
    ~Input() = default;
};

}  // namespace Pathfinder
