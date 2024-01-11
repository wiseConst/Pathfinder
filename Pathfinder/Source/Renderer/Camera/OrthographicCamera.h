#ifndef ORTHOGRAPHICCAMERA_H
#define ORTHOGRAPHICCAMERA_H

#include "Camera.h"

namespace Pathfinder
{

class OrthographicCamera final : public Camera
{
  public:
    OrthographicCamera()           = default;
    ~OrthographicCamera() override = default;

  private:
};

}  // namespace Pathfinder

#endif  // ORTHOGRAPHICCAMERA_H
