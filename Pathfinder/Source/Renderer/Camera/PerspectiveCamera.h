#ifndef PERSPECTIVECAMERA_H
#define PERSPECTIVECAMERA_H

#include "Camera.h"

namespace Pathfinder
{

// TODO: Refactor
class PerspectiveCamera final : public Camera
{
  public:
    PerspectiveCamera()           = default;
    ~PerspectiveCamera() override = default;

  protected:
    void OnEvent(Event& e) final override {}

    void OnUpdate(const float deltaTime) final override {}

    void RecalculateViewMatrix() final override {}
};

}  // namespace Pathfinder

#endif  // PERSPECTIVECAMERA_H
