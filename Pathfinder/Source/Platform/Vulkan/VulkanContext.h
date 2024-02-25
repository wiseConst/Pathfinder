#ifndef VULKANCONTEXT_H
#define VULKANCONTEXT_H

#include "Renderer/GraphicsContext.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanDevice;

class VulkanContext final : public GraphicsContext
{
  public:
    VulkanContext() noexcept;
    ~VulkanContext() override;

    static FORCEINLINE NODISCARD VulkanContext& Get()
    {
        PFR_ASSERT(s_Instance, "Graphics context instance is not valid!");
        return static_cast<VulkanContext&>(*s_Instance);
    }

    FORCEINLINE const auto& GetDevice() const { return m_Device; }
    FORCEINLINE const auto& GetInstance() const { return m_VulkanInstance; }


  private:
    VkInstance m_VulkanInstance               = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

    Unique<VulkanDevice> m_Device;

    void Destroy() final override;
    void CreateInstance();
    void CreateDebugMessenger();

    bool CheckVulkanAPISupport() const;
    bool CheckVulkanValidationSupport() const;
    std::vector<const char*> GetRequiredExtensions() const;
};

}  // namespace Pathfinder

#endif  // VULKANCONTEXT_H
