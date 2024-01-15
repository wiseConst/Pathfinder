#ifndef INPUT_H
#define INPUT_H

#include "Core.h"
#include "Keys.h"

namespace Pathfinder
{

class Input final : private Uncopyable, private Unmovable
{
  public:
    // TODO: implement other functions like is mouse button pressed

    static bool IsKeyPressed(const EKey key);
    static bool IsKeyReleased(const EKey key);
    static bool IsKeyRepeated(const EKey key);

  private:
    Input()           = default;
    ~Input() override = default;
};

}  // namespace Pathfinder

#endif  // INPUT_H
