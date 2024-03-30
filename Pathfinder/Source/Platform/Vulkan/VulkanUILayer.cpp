#include "PathfinderPCH.h"
#include "VulkanUILayer.h"

#include "Events/Events.h"
#include "Events/MouseEvent.h"

#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "VulkanDevice.h"
#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"
#include "VulkanFramebuffer.h"

#include "Renderer/Swapchain.h"
#include "Renderer/Renderer.h"

#include "Core/Application.h"
#include "Core/Window.h"

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

    {
        FramebufferSpecification framebufferSpec = {"UIFramebuffer"};
        const auto& compositeFramebuffer         = Renderer::GetRendererData()->CompositeFramebuffer;
        framebufferSpec.ExistingAttachments[0]   = compositeFramebuffer->GetAttachments()[0].Attachment;

        FramebufferAttachmentSpecification attachmentSpec = compositeFramebuffer->GetAttachments()[0].Specification;
        attachmentSpec.LoadOp                             = ELoadOp::LOAD;
        attachmentSpec.StoreOp                            = EStoreOp::STORE;
        framebufferSpec.Attachments.emplace_back(attachmentSpec);

        m_UIFramebuffer = Framebuffer::Create(framebufferSpec);
        Application::Get().GetWindow()->AddResizeCallback([&](uint32_t width, uint32_t height) { m_UIFramebuffer->Resize(width, height); });
    }

    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
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

    VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets                    = 1000;
    poolInfo.poolSizeCount              = (uint32_t)std::size(poolSizes);
    poolInfo.pPoolSizes                 = poolSizes;

    VK_CHECK(vkCreateDescriptorPool(device->GetLogicalDevice(), &poolInfo, nullptr, &m_ImGuiPool), "Failed to create ImGui Pool!");

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_DockingEnable |
                      ImGuiConfigFlags_DpiEnableScaleFonts | ImGuiConfigFlags_DpiEnableScaleViewports;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    PFR_ASSERT(
        ImGui_ImplVulkan_LoadFunctions([](const char* functionName, void* vulkanInstance)
                                       { return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkanInstance)), functionName); },
                                       (void*)&context.GetInstance()),
        "Failed to load functions into ImGui!");

    const auto& window = Application::Get().GetWindow();
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window->Get(), true);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance                  = context.GetInstance();
    initInfo.PhysicalDevice            = device->GetPhysicalDevice();
    initInfo.Device                    = device->GetLogicalDevice();
    initInfo.Queue                     = device->GetGraphicsQueue();
    initInfo.DescriptorPool            = m_ImGuiPool;
    const auto& swapchain              = window->GetSwapchain();
    initInfo.MinImageCount = initInfo.ImageCount = swapchain->GetImageCount();
    initInfo.MSAASamples                         = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn                     = [](VkResult err) { VK_CHECK(err, "ImGui issues!"); };

    initInfo.UseDynamicRendering                              = true;
    initInfo.PipelineRenderingCreateInfo                      = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;

    const auto vkImageFormat = ImageUtils::PathfinderImageFormatToVulkan(Renderer::GetFinalPassImage()->GetSpecification().Format);
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &vkImageFormat;

    ImGui_ImplVulkan_Init(&initInfo);

    // execute a gpu command to upload imgui font textures
    ImGui_ImplVulkan_CreateFontsTexture();
}

void VulkanUILayer::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_ImGuiPool, nullptr);
}

void VulkanUILayer::OnEvent(Event& e)
{
#if 0
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<MouseMovedEvent>([](const MouseMovedEvent& e) -> bool { return ImGui::IsAnyItemHovered(); });
    dispatcher.Dispatch<MouseScrolledEvent>([](const MouseScrolledEvent& e) -> bool { return ImGui::IsAnyItemHovered(); });
    dispatcher.Dispatch<MouseButtonPressedEvent>([](const MouseButtonPressedEvent& e) -> bool { return ImGui::IsAnyItemHovered(); });
    dispatcher.Dispatch<MouseButtonReleasedEvent>([](const MouseButtonReleasedEvent& e) -> bool { return ImGui::IsAnyItemHovered(); });
#endif
}

void VulkanUILayer::BeginRender()
{
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void VulkanUILayer::EndRender()
{
    static const auto& rd = Renderer::GetRendererData();

    m_UIFramebuffer->BeginPass(rd->RenderCommandBuffer[rd->FrameIndex]);

    // Rendering
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)rd->RenderCommandBuffer[rd->FrameIndex]->Get());

    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    m_UIFramebuffer->EndPass(rd->RenderCommandBuffer[rd->FrameIndex]);
}

}  // namespace Pathfinder