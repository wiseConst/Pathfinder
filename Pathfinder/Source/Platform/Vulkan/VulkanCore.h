#ifndef VULKANCORE_H
#define VULKANCORE_H

#include <volk.h>

namespace Pathfinder
{

#define VK_FORCE_VALIDATION 0
#define VK_LOG_INFO 1

#if PFR_DEBUG
constexpr static bool s_bEnableValidationLayers = true;
#else
constexpr static bool s_bEnableValidationLayers = false;
#endif

static VkBool32 DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{

    return VK_FALSE;
}

}  // namespace Pathfinder

#endif  // VULKANCORE_H
