#include <PathfinderPCH.h>
#include "VulkanContext.h"

#include "VulkanDevice.h"
#include "VulkanAllocator.h"

#include <Core/Application.h>
#include <Core/Window.h>

namespace Pathfinder
{

VulkanContext::VulkanContext() noexcept
{
    s_Instance = this;

    CreateInstance();
    CreateDebugMessenger();

    m_Device = MakeUnique<VulkanDevice>(m_VulkanInstance);
    LOG_INFO("{}", __FUNCTION__);
}

VulkanContext::~VulkanContext()
{
    Destroy();
    LOG_INFO("{}", __FUNCTION__);
}

NODISCARD const float VulkanContext::GetTimestampPeriod() const
{
    return m_Device->GetTimestampPeriod();
}

void VulkanContext::CreateInstance()
{
    PFR_ASSERT(volkInitialize() == VK_SUCCESS, "Failed to initialize volk( meta-loader for Vulkan )!");
    PFR_ASSERT(CheckVulkanAPISupport(), "Vulkan is not supported!");

    if constexpr (VK_FORCE_VALIDATION || s_bEnableValidationLayers)
        PFR_ASSERT(CheckVulkanValidationSupport(), "Validation layers aren't supported!");

    uint32_t supportedApiVersionFromDLL = 0;
    VK_CHECK(vkEnumerateInstanceVersion(&supportedApiVersionFromDLL), "Failed to retrieve supported vulkan version!");

#if VK_LOG_INFO
    LOG_INFO("System supports vulkan version up to: {}.{}.{}.{}", VK_API_VERSION_VARIANT(supportedApiVersionFromDLL),
             VK_API_VERSION_MAJOR(supportedApiVersionFromDLL), VK_API_VERSION_MINOR(supportedApiVersionFromDLL),
             VK_API_VERSION_PATCH(supportedApiVersionFromDLL));
#endif
    PFR_ASSERT(PFR_VK_API_VERSION <= supportedApiVersionFromDLL, "Desired VK version >= available VK version.");

    const VkApplicationInfo applicationInfo = {.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                               .pApplicationName   = Application::Get().GetSpecification().Title.data(),
                                               .applicationVersion = VK_MAKE_API_VERSION(0, 1, 1, 0),
                                               .pEngineName        = s_ENGINE_NAME.data(),
                                               .engineVersion      = VK_MAKE_API_VERSION(0, 1, 1, 0),
                                               .apiVersion         = supportedApiVersionFromDLL};

    const auto enabledInstanceExtensions = GetRequiredExtensions();
    VkInstanceCreateInfo instanceCI      = {.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                            .pApplicationInfo        = &applicationInfo,
                                            .enabledExtensionCount   = static_cast<uint32_t>(enabledInstanceExtensions.size()),
                                            .ppEnabledExtensionNames = enabledInstanceExtensions.data()};

    if constexpr (VK_FORCE_VALIDATION || s_bEnableValidationLayers)
    {
        instanceCI.enabledLayerCount   = static_cast<uint32_t>(s_InstanceLayers.size());
        instanceCI.ppEnabledLayerNames = s_InstanceLayers.data();
    }

#if VK_SHADER_DEBUG_PRINTF
    constexpr VkValidationFeatureEnableEXT validationFeatureToEnable = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
    VkValidationFeaturesEXT validationInfo                           = {.sType                         = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
                                                                        .pEnabledValidationFeatures    = &validationFeatureToEnable,
                                                                        .enabledValidationFeatureCount = 1};

    instanceCI.pNext = &validationInfo;
#endif

    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &m_VulkanInstance), "Failed to create vulkan instance!");
    volkLoadInstanceOnly(m_VulkanInstance);

#if PFR_DEBUG
    LOG_INFO("Using vulkan version: {}.{}.{}.{}", VK_API_VERSION_VARIANT(supportedApiVersionFromDLL),
             VK_API_VERSION_MAJOR(supportedApiVersionFromDLL), VK_API_VERSION_MINOR(supportedApiVersionFromDLL),
             VK_API_VERSION_PATCH(supportedApiVersionFromDLL));

    LOG_TRACE("Enabled instance extensions:");
    for (const auto& ext : enabledInstanceExtensions)
        LOG_TRACE("  {}", ext);

    LOG_TRACE("Enabled instance layers:");
    for (const auto& layer : s_InstanceLayers)
        LOG_TRACE("  {}", layer);
#endif
}

void VulkanContext::CreateDebugMessenger()
{
    if constexpr (!s_bEnableValidationLayers && !VK_FORCE_VALIDATION) return;

    constexpr VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =  //
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,  //
        .pfnUserCallback = &VK_DebugCallback,                                                                          //
        .pUserData       = nullptr                                                                                     // pUserData
    };
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_VulkanInstance, &debugMessengerCI, nullptr, &m_DebugMessenger),
             "Failed to create debug messenger!");
}

bool VulkanContext::CheckVulkanAPISupport() const
{
    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr),
             "Can't retrieve number of supported vulkan instance extensions.");

    std::vector<VkExtensionProperties> extensions(extensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()),
             "Can't retrieve supported vulkan instance extensions.");

#if VK_LOG_INFO && PFR_DEBUG
    LOG_INFO("List of supported vulkan INSTANCE extensions. [{}]", extensionCount);
    for (const auto& [extensionName, specVersion] : extensions)
    {
        LOG_TRACE(" {}, version: {}.{}.{}", extensionName, VK_API_VERSION_MAJOR(specVersion), VK_API_VERSION_MINOR(specVersion),
                  VK_API_VERSION_PATCH(specVersion));
    }
#endif

    const auto wsiExtensions = Window::GetWSIExtensions();

#if PFR_DEBUG && VK_LOG_INFO
    LOG_INFO("List of extensions required by window {}:", wsiExtensions.size());

    for (auto& ext : wsiExtensions)
        LOG_TRACE(" {}", ext);
#endif

    // Check if current vulkan version supports required by window extensions
    for (const auto& wsiExt : wsiExtensions)
    {
        bool bIsSupported = false;
        for (const auto& [instanceExt, specVersion] : extensions)
        {
            if (strcmp(instanceExt, wsiExt) != 0) continue;

            bIsSupported = true;
            break;
        }

        if (!bIsSupported) return false;
    }

    return true;
}

bool VulkanContext::CheckVulkanValidationSupport() const
{
    uint32_t layerCount = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr), "Can't retrieve number of supported vulkan instance layers.");

    std::vector<VkLayerProperties> availableLayers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()), "Can't retrieve supported vulkan instance layers.");

#if VK_LOG_INFO && PFR_DEBUG
    LOG_INFO("List of supported vulkan INSTANCE layers. [{}]", layerCount);
    for (const auto& [layerName, specVersion, implementationVersion, description] : availableLayers)
    {
        LOG_TRACE("{}", description);
        LOG_TRACE("\t{}, version: {}.{}.{}", layerName, VK_API_VERSION_MAJOR(specVersion), VK_API_VERSION_MINOR(specVersion),
                  VK_API_VERSION_PATCH(specVersion));
    }
#endif

    for (const auto& requestedLayer : s_InstanceLayers)
    {
        bool bIsSupported = false;
        for (const auto& availableLayer : availableLayers)
        {
            if (strcmp(availableLayer.layerName, requestedLayer) != 0) continue;

            bIsSupported = true;
            break;
        }

        if (!bIsSupported) return false;
    }

    return true;
}

std::vector<const char*> VulkanContext::GetRequiredExtensions() const
{
    auto extensions = Window::GetWSIExtensions();
    extensions.insert(extensions.end(), s_InstanceExtensions.begin(), s_InstanceExtensions.end());

    if constexpr (s_bEnableValidationLayers || VK_FORCE_VALIDATION) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return extensions;
}

void VulkanContext::WaitDeviceOnFinish() const
{
    m_Device->WaitDeviceOnFinish();
}

void VulkanContext::Begin()
{
    m_Device->ResetCommandPools();
}

void VulkanContext::End() {}

void VulkanContext::FillMemoryBudgetStats(std::vector<MemoryBudget>& memoryBudgets)
{
    m_Device->GetAllocator()->FillMemoryBudgetStats(memoryBudgets);
}

void VulkanContext::Destroy()
{
    m_Device->WaitDeviceOnFinish();

    m_Device.reset();

    if constexpr (VK_FORCE_VALIDATION || s_bEnableValidationLayers)
        vkDestroyDebugUtilsMessengerEXT(m_VulkanInstance, m_DebugMessenger, nullptr);

    vkDestroyInstance(m_VulkanInstance, nullptr);
    volkFinalize();
}

}  // namespace Pathfinder