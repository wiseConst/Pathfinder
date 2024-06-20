#include <PathfinderPCH.h>
#include "VulkanUILayer.h"

#include <Events/Events.h>
#include <Events/MouseEvent.h>

#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "VulkanDevice.h"
#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"
#include "VulkanTexture.h"

#include <Renderer/Swapchain.h>
#include <Renderer/Renderer.h>

#include <Core/Application.h>
#include <Core/Window.h>

namespace Pathfinder
{

VulkanUILayer::VulkanUILayer()
{
    Init();
}

VulkanUILayer::~VulkanUILayer()
{
    Destroy();
}

void VulkanUILayer::Init()
{
    const auto& context = VulkanContext::Get();
    const auto& device  = context.GetDevice();

    constexpr VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                                  {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                                  {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                                  {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                                  {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                                  {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                                  {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    const VkDescriptorPoolCreateInfo poolInfo = {.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                 .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                                 .maxSets       = s_MAX_TEXTURES,
                                                 .poolSizeCount = (uint32_t)std::size(poolSizes),
                                                 .pPoolSizes    = poolSizes};
    VK_CHECK(vkCreateDescriptorPool(device->GetLogicalDevice(), &poolInfo, nullptr, &m_ImGuiPool), "Failed to create ImGui Pool!");

    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_DockingEnable |
                      ImGuiConfigFlags_DpiEnableScaleFonts | ImGuiConfigFlags_DpiEnableScaleViewports;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    io.WantCaptureKeyboard = true;
    io.WantCaptureMouse    = true;
    io.WantTextInput       = true;

    PFR_ASSERT(
        ImGui_ImplVulkan_LoadFunctions([](const char* functionName, void* vulkanInstance)
                                       { return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkanInstance)), functionName); },
                                       (void*)&context.GetInstance()),
        "Failed to load functions into ImGui!");

    const auto& window = Application::Get().GetWindow();
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window->Get(), true);

    const auto& swapchain              = window->GetSwapchain();
    const auto vkImageFormat           = ImageUtils::PathfinderImageFormatToVulkan(swapchain->GetImageFormat());
    ImGui_ImplVulkan_InitInfo initInfo = {
        .Instance                    = context.GetInstance(),
        .PhysicalDevice              = device->GetPhysicalDevice(),
        .Device                      = device->GetLogicalDevice(),
        .QueueFamily                 = device->GetGraphicsFamily(),
        .Queue                       = device->GetGraphicsQueue(),
        .DescriptorPool              = m_ImGuiPool,
        .MinImageCount               = swapchain->GetImageCount(),
        .ImageCount                  = swapchain->GetImageCount(),
        .MSAASamples                 = VK_SAMPLE_COUNT_1_BIT,
        .PipelineCache               = device->GetPipelineCache(),
        .UseDynamicRendering         = true,
        .PipelineRenderingCreateInfo = {.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                        .colorAttachmentCount    = 1,
                                        .pColorAttachmentFormats = &vkImageFormat},
        .CheckVkResultFn             = [](VkResult err) { VK_CHECK(err, "ImGui issues!"); },
    };
    ImGui_ImplVulkan_Init(&initInfo);

    // execute a gpu command to upload imgui font textures
    ImGui_ImplVulkan_CreateFontsTexture();

    window->AddResizeCallback(
        [](const WindowResizeData& resizeData)
        {
            for (auto& [uuid, textureID] : s_TextureIDMap)
            {
                ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)textureID);
            }
            s_LastActiveTextures.clear();
            s_TextureIDMap.clear();
        });

    SetCustomTheme();
}

void VulkanUILayer::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    for (auto& [image, textureID] : s_TextureIDMap)
    {
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)textureID);
    }
    s_TextureIDMap.clear();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_ImGuiPool, nullptr);
}

void VulkanUILayer::OnEvent(Event& e)
{
    //   if (!m_bBlockEvents) return;

    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<MouseMovedEvent>([](const MouseMovedEvent& e) -> bool { return m_bBlockEvents; });
    dispatcher.Dispatch<MouseScrolledEvent>([](const MouseScrolledEvent& e) -> bool { return m_bBlockEvents; });
    dispatcher.Dispatch<MouseButtonPressedEvent>([](const MouseButtonPressedEvent& e) -> bool { return m_bBlockEvents; });
    dispatcher.Dispatch<MouseButtonReleasedEvent>([](const MouseButtonReleasedEvent& e) -> bool { return m_bBlockEvents; });
}

void VulkanUILayer::BeginRender()
{
    UpdateTextureIDs();

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void VulkanUILayer::EndRender()
{
    static const auto& rd = Renderer::GetRendererData();

    const auto& swapchain = Application::Get().GetWindow()->GetSwapchain();
    swapchain->BeginPass(rd->RenderCommandBuffer[rd->FrameIndex]);

    // Rendering
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)rd->RenderCommandBuffer[rd->FrameIndex]->Get());

    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    swapchain->EndPass(rd->RenderCommandBuffer[rd->FrameIndex]);
}

void VulkanUILayer::UpdateTextureIDs()
{
    for (auto& [uuid, bIsActive] : s_LastActiveTextures)
    {
        if (bIsActive)
        {
            bIsActive = false;
            continue;
        }

        const auto& textureID = s_TextureIDMap[uuid];
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)textureID);
        s_TextureIDMap.erase(uuid);
    }
}

void UILayer::DrawTexture(Shared<Texture> texture, const glm::vec2& size, const glm::vec2& uv0, const glm::vec2& uv1,
                        const glm::vec4& tintCol, const glm::vec4& borderCol)
{
    PFR_ASSERT(texture, "UILayer::DrawTexture() - Texture is null!");

    const auto& uuid = texture->GetUUID();
    if (!s_TextureIDMap.contains(uuid))
    {
        s_TextureIDMap.emplace(uuid, nullptr);
        auto& textureID = s_TextureIDMap[uuid];

            const auto vulkanTexture = std::static_pointer_cast<VulkanTexture>(texture);
        PFR_ASSERT(vulkanTexture, "Failed to cast Texture to VulkanTexture!");


        const auto& vkImageInfo = vulkanTexture->GetDescriptorInfo();
        textureID               = ImGui_ImplVulkan_AddTexture(vkImageInfo.sampler, vkImageInfo.imageView, vkImageInfo.imageLayout);
        PFR_ASSERT(textureID, "Failed to receieve textureID from imgui!");

        ImGui::Image(textureID, (ImVec2&)size, (ImVec2&)uv0, (ImVec2&)uv1, (ImVec4&)tintCol, (ImVec4&)borderCol);
        s_LastActiveTextures[uuid] = true;
    }
    else
    {
        const auto& textureID = s_TextureIDMap[uuid];
        ImGui::Image(textureID, (ImVec2&)size, (ImVec2&)uv0, (ImVec2&)uv1, (ImVec4&)tintCol, (ImVec4&)borderCol);

        s_LastActiveTextures[uuid] = true;
    }
}

}  // namespace Pathfinder