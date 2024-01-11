#include "PathfinderPCH.h"
#include "Camera.h"

#include "OrthographicCamera.h"

namespace Pathfinder
{

Shared<Camera> Camera::Create(const ECameraType cameraType)
{
    switch (cameraType)
    {
        case ECameraType::CAMERA_TPYE_ORTHOGRAPHIC: return MakeShared<OrthographicCamera>();
    }

    PFR_ASSERT(false, "Unknown camera type!");
    return nullptr;
}

}  // namespace Pathfinder