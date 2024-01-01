#include "PathfinderPCH.h"
#include "VulkanShader.h"

namespace Pathfinder
{

VulkanShader::VulkanShader(const std::string_view shaderPath)
{
    LOG_TAG(VULKAN, "Loading shader \"%s\"...", shaderPath.data());

}

void VulkanShader::Destroy() {
}

}  // namespace Pathfinder