#include "PathfinderPCH.h"
#include "VulkanPipeline.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"

namespace Pathfinder
{

VulkanPipeline::VulkanPipeline(const PipelineSpecification& pipelineSpec) : m_Specification(pipelineSpec) {}

void VulkanPipeline::Invalidate() {}

void VulkanPipeline::Destroy()
{
    auto& context             = VulkanContext::Get();
    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();

    vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);
    vkDestroyPipeline(logicalDevice, m_Handle, nullptr);
}

}  // namespace Pathfinder