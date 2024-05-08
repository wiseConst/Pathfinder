#include "PathfinderPCH.h"
#include "VulkanContext.h"

#include "VulkanDevice.h"
#include "VulkanDeviceManager.h"
#include "VulkanAllocator.h"

#include "Core/Application.h"
#include "Core/Window.h"

namespace Pathfinder
{

VulkanContext::VulkanContext() noexcept
{
    s_Instance = this;

    CreateInstance();
    CreateDebugMessenger();

    m_Device = VulkanDeviceManager::ChooseBestFitDevice(m_VulkanInstance);
}

VulkanContext::~VulkanContext()
{
    Destroy();
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
    LOG_INFO("System supports vulkan version up to: %u.%u.%u.%u", VK_API_VERSION_VARIANT(supportedApiVersionFromDLL),
             VK_API_VERSION_MAJOR(supportedApiVersionFromDLL), VK_API_VERSION_MINOR(supportedApiVersionFromDLL),
             VK_API_VERSION_PATCH(supportedApiVersionFromDLL));
#endif
    PFR_ASSERT(PFR_VK_API_VERSION <= supportedApiVersionFromDLL, "Desired VK version >= available VK version.");

    VkApplicationInfo applicationInfo  = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.apiVersion         = supportedApiVersionFromDLL;
    applicationInfo.pApplicationName   = Application::Get().GetSpecification().Title.data();
    applicationInfo.pEngineName        = s_ENGINE_NAME.data();
    applicationInfo.engineVersion      = VK_MAKE_API_VERSION(0, 1, 1, 0);
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 1, 0);

    VkInstanceCreateInfo instanceCI = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCI.pApplicationInfo     = &applicationInfo;

    const auto enabledInstanceExtensions = GetRequiredExtensions();
    instanceCI.enabledExtensionCount     = static_cast<uint32_t>(enabledInstanceExtensions.size());
    instanceCI.ppEnabledExtensionNames   = enabledInstanceExtensions.data();

    if constexpr (VK_FORCE_VALIDATION || s_bEnableValidationLayers)
    {
        instanceCI.enabledLayerCount   = static_cast<uint32_t>(s_InstanceLayers.size());
        instanceCI.ppEnabledLayerNames = s_InstanceLayers.data();
    }

#if VK_SHADER_DEBUG_PRINTF
    VkValidationFeaturesEXT validationInfo                           = {VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    constexpr VkValidationFeatureEnableEXT validationFeatureToEnable = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
    validationInfo.enabledValidationFeatureCount                     = 1;
    validationInfo.pEnabledValidationFeatures                        = &validationFeatureToEnable;

    instanceCI.pNext = &validationInfo;
#endif

    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &m_VulkanInstance), "Failed to create vulkan instance!");
    volkLoadInstanceOnly(m_VulkanInstance);

#if PFR_DEBUG
    LOG_INFO("Using vulkan version: %u.%u.%u.%u", VK_API_VERSION_VARIANT(supportedApiVersionFromDLL),
             VK_API_VERSION_MAJOR(supportedApiVersionFromDLL), VK_API_VERSION_MINOR(supportedApiVersionFromDLL),
             VK_API_VERSION_PATCH(supportedApiVersionFromDLL));

    LOG_TAG_TRACE(VULKAN, "Enabled instance extensions:");
    for (const auto& ext : enabledInstanceExtensions)
        LOG_TAG_TRACE(VULKAN, "  %s", ext);

    LOG_TAG_TRACE(VULKAN, "Enabled instance layers:");
    for (const auto& layer : s_InstanceLayers)
        LOG_TAG_TRACE(VULKAN, "  %s", layer);
#endif
}

void VulkanContext::CreateDebugMessenger()
{
    if constexpr (!s_bEnableValidationLayers && !VK_FORCE_VALIDATION) return;

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debugMessengerCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
    debugMessengerCreateInfo.pfnUserCallback = &DebugCallback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_VulkanInstance, &debugMessengerCreateInfo, nullptr, &m_DebugMessenger),
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
    LOG_INFO("List of supported vulkan INSTANCE extensions. (%u)", extensionCount);
    for (const auto& [extensionName, specVersion] : extensions)
    {
        LOG_TRACE(" %s, version: %u.%u.%u", extensionName, VK_API_VERSION_MAJOR(specVersion), VK_API_VERSION_MINOR(specVersion),
                  VK_API_VERSION_PATCH(specVersion));
    }
#endif

    const auto wsiExtensions = Window::GetWSIExtensions();

#if PFR_DEBUG && VK_LOG_INFO
    LOG_INFO("List of extensions required by window (%zu):", wsiExtensions.size());

    for (auto& ext : wsiExtensions)
        LOG_TRACE(" %s", ext);
#endif

    // Check if current vulkan version supports required by window extensions
    for (const auto& wsiExt : wsiExtensions)
    {
        bool bIsSupported = false;
        for (const auto& [instanceExt, specVersion] : extensions)
        {
            if (strcmp(instanceExt, wsiExt) == 0)
            {
                bIsSupported = true;
                break;
            }
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
    LOG_INFO("List of supported vulkan INSTANCE layers. (%u)", layerCount);
    for (const auto& [layerName, specVersion, implementationVersion, description] : availableLayers)
    {
        LOG_TRACE("%s", description);
        LOG_TRACE("\t%s, version: %u.%u.%u", layerName, VK_API_VERSION_MAJOR(specVersion), VK_API_VERSION_MINOR(specVersion),
                  VK_API_VERSION_PATCH(specVersion));
    }
#endif

    for (const auto& requestedLayer : s_InstanceLayers)
    {
        bool bIsSupported = false;
        for (const auto& availableLayer : availableLayers)
        {
            if (strcmp(availableLayer.layerName, requestedLayer) == 0)
            {
                bIsSupported = true;
                break;
            }
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