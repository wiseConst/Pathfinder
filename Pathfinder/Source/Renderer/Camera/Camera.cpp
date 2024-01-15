#include "PathfinderPCH.h"
#include "Camera.h"

#include "OrthographicCamera.h"
#include "PerspectiveCamera.h"

namespace Pathfinder
{

Shared<Camera> Camera::Create(const ECameraType cameraType)
{
    switch (cameraType)
    {
        case ECameraType::CAMERA_TYPE_ORTHOGRAPHIC: return MakeShared<OrthographicCamera>();
        case ECameraType::CAMERA_TYPE_PERSPECTIVE: return MakeShared<PerspectiveCamera>();
    }

    PFR_ASSERT(false, "Unknown camera type!");
    return nullptr;
}

}  // namespace Pathfinder