#ifndef VULKANCONTEXT_H
#define VULKANCONTEXT_H

#include "Renderer/GraphicsContext.h"

#include "VulkanCore.h"
#include <volk/volk.h>

namespace Pathfinder
{

class VulkanContext final : public GraphicsContext
{
  public:
    VulkanContext() noexcept;
    ~VulkanContext() override = default;

    static FORCEINLINE NODISCARD VulkanContext& Get()
    {
        PFR_ASSERT(s_Instance, "Graphics context instance is not valid!");
        return static_cast<VulkanContext&>(*s_Instance);
    }

    void Destroy() final override;

  private:
    VkInstance m_VulkanInstance               = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

    void CreateInstance();
    void CreateDebugMessenger();

    VkBool32 CheckVulkanAPISupport() const;
    VkBool32 CheckVulkanValidationSupport() const;
    std::vector<const char*> GetRequiredExtensions() const;
};

}  // namespace Pathfinder

#endif  // VULKANCONTEXT_H
