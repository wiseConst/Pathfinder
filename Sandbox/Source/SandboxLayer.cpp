#include "SandboxLayer.h"
#include <Panels/SceneHierarchyPanel.h>

namespace Pathfinder
{
static const std::string sceneFilePath = "Scenes/SponzaSonne";

SandboxLayer::SandboxLayer() : Layer("SandboxLayer") {}
SandboxLayer::~SandboxLayer() = default;

void SandboxLayer::Init()
{
    Application::Get().GetWindow()->SetIconImage("Other/Logo.png");

    m_Camera = Camera::Create(ECameraType::CAMERA_TYPE_PERSPECTIVE);

    m_ActiveScene = MakeShared<Scene>(s_DEFAULT_STRING);
    SceneManager::Deserialize(m_ActiveScene, sceneFilePath);
    m_WorldOutlinerPanel = MakeUnique<SceneHierarchyPanel>(m_ActiveScene);

    UILayer::SetDefaultFont("Fonts/Manrope/static/Manrope-Bold.ttf", 18.0f);
}

void SandboxLayer::Destroy()
{
    SceneManager::Serialize(m_ActiveScene, sceneFilePath);
}

void SandboxLayer::OnEvent(Event& e)
{
    m_Camera->OnEvent(e);

    static bool bF3WasPressed = false;

    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyButtonReleasedEvent>(
        [&](const KeyButtonReleasedEvent& event) -> bool
        {
            if (event.GetModKey() == EModKey::MOD_KEY_ALT && bF3WasPressed && Input::IsKeyReleased(EKey::KEY_F3))
            {
                bRenderUI = !bRenderUI;
                return true;
            }

            const auto key = event.GetKey();
            if (key == EKey::KEY_F1)
            {
                Application::Get().GetWindow()->SetWindowMode(EWindowMode::WINDOW_MODE_WINDOWED);
                return true;
            }

            if (key == EKey::KEY_F2)
            {
                Application::Get().GetWindow()->SetWindowMode(EWindowMode::WINDOW_MODE_BORDERLESS_FULLSCREEN);

                return true;
            }

            if (key == EKey::KEY_F3)
            {
                Application::Get().GetWindow()->SetWindowMode(EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE);

                return true;
            }

            return true;
        });

    bF3WasPressed = Input::IsKeyPressed(EKey::KEY_F3);
}

void SandboxLayer::OnUpdate(const float deltaTime)
{
    m_Camera->OnUpdate(deltaTime);

    Renderer::BeginScene(*m_Camera);

    const glm::vec3 minLightPos{-15, -5, -5};
    const glm::vec3 maxLightPos{15, 20, 5};

    m_ActiveScene->ForEach<TransformComponent, PointLightComponent>(
        [&](TransformComponent& tc, PointLightComponent& plc)
        {
            tc.Translation += glm::vec3(0, 3.0f, 0) * deltaTime;
            if (tc.Translation.y > maxLightPos.y)
            {
                tc.Translation.y -= (maxLightPos.y - minLightPos.y);
            }
        });

    m_ActiveScene->OnUpdate(deltaTime);

    Renderer::EndScene();

    if (Input::IsKeyPressed(EKey::KEY_ESCAPE) && Input::IsKeyPressed(EKey::KEY_LEFT_SHIFT))
    {
        Application::Close();
    }
}

void SandboxLayer::OnUIRender()
{
    if (!bRenderUI)
    {
        UILayer::SetBlockEvents(false);
        return;
    }

    bool bAnythingHovered = false;
    bool bAnythingFocused = false;

    static bool bShowDemoWindow = true;
    if (bShowDemoWindow)
    {
        ImGui::ShowDemoWindow(&bShowDemoWindow);
        bAnythingHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered();
        bAnythingFocused = ImGui::IsAnyItemFocused() || ImGui::IsWindowFocused();
    }

    static bool bShowRendererSettings = true;
    if (bShowRendererSettings)
    {
        ImGui::Begin("Renderer Settings", &bShowRendererSettings);

        auto& rs              = Renderer::GetRendererSettings();
        const bool bIsChecked = ImGui::Checkbox("VSync", &rs.bVSync);
        ImGui::Separator();

        ImGui::Checkbox("Draw Colliders", &rs.bDrawColliders);
        ImGui::Separator();

        const auto& mainWindowSwapchain   = Application::Get().GetWindow()->GetSwapchain();
        const char* items[3]              = {"FIFO", "IMMEDIATE", "MAILBOX"};
        const auto currentPresentMode     = mainWindowSwapchain->GetPresentMode();
        const char* currentPresentModeStr = currentPresentMode == EPresentMode::PRESENT_MODE_FIFO        ? items[0]
                                            : currentPresentMode == EPresentMode::PRESENT_MODE_IMMEDIATE ? items[1]
                                                                                                         : items[2];
        ImGui::Text("PresentMode");
        if (!bIsChecked && ImGui::BeginCombo("##presentModeCombo", currentPresentModeStr))
        {
            uint32_t presentModeIndex = UINT32_MAX;
            for (uint32_t n{}; n < IM_ARRAYSIZE(items); ++n)
            {
                const auto bIsSeleceted = strcmp(currentPresentModeStr, items[n]) == 0;
                if (ImGui::Selectable(items[n], bIsSeleceted))
                {
                    currentPresentModeStr = items[n];
                    presentModeIndex      = n;
                }
                if (bIsSeleceted) ImGui::SetItemDefaultFocus();
            }

            if (presentModeIndex != UINT32_MAX)
            {
                rs.bVSync = presentModeIndex == 0;

                mainWindowSwapchain->SetPresentMode(presentModeIndex == 0   ? EPresentMode::PRESENT_MODE_FIFO
                                                    : presentModeIndex == 1 ? EPresentMode::PRESENT_MODE_IMMEDIATE
                                                                            : EPresentMode::PRESENT_MODE_MAILBOX);
            }

            ImGui::EndCombo();
        }
        ImGui::Separator();

        bAnythingHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered();
        bAnythingFocused = ImGui::IsAnyItemFocused() || ImGui::IsWindowFocused();

        ImGui::End();
    }

    static bool bShowRenderTargetList = true;
    if (bShowRenderTargetList)
    {
        ImGui::Begin("Renderer Target List", &bShowRenderTargetList);

        for (const auto& [name, image] : Renderer::GetRenderTargetList())
        {
            ImGui::Text("%s", name.data());
            const uint32_t imageWidth =
                image->GetSpecification().Width > 10 ? image->GetSpecification().Width / 4 : image->GetSpecification().Width * 100;
            const uint32_t imageHeight =
                image->GetSpecification().Height > 10 ? image->GetSpecification().Height / 4 : image->GetSpecification().Height * 100;
            UILayer::DrawImage(image, {imageWidth, imageHeight}, {0, 0}, {1, 1});
            ImGui::Separator();
        }

        bAnythingHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered();
        bAnythingFocused = ImGui::IsAnyItemFocused() || ImGui::IsWindowFocused();

        ImGui::End();
    }

    static bool bShowPipelineMap = true;
    if (bShowPipelineMap)
    {
        ImGui::Begin("Pipeline Map", &bShowPipelineMap);

        // TODO: Make ui better: kind of ALIGNED Table?
        // NAME|ACTION
        // DepthPrePass | RELOAD
        // SSAO         | RELOAD
        for (const auto& [hash, pipeline] : PipelineLibrary::GetStorage())
        {
            const auto& pipelineSpec = pipeline->GetSpecification();
            ImGui::PushID(pipelineSpec.DebugName.data());
            ImGui::Text("%s", pipelineSpec.DebugName.data());
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 100.0f);
            if (ImGui::Button("Hot-Reload"))
            {
                LOG_WARN("Hot-reloading pipeline: {}", pipelineSpec.DebugName);
                PipelineLibrary::Invalidate(hash);
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        bAnythingHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered();
        bAnythingFocused = ImGui::IsAnyItemFocused() || ImGui::IsWindowFocused();

        ImGui::End();
    }

    static bool bShowRendererStats = true;
    if (bShowRendererStats)
    {
        const auto& rs = Renderer::GetStats();
        ImGui::Begin("Renderer Statistics", &bShowRendererStats);

        ImGui::Separator();
        ImGui::Text("ImageViews: %u", rs.ImageViewCount);

        ImGui::SeparatorText("Pass Statistics");
        for (const auto& [pass, passTime] : Renderer::GetPassStatistics())
            ImGui::Text("%s: %0.4f(ms)", pass.data(), passTime);

        ImGui::SeparatorText("Pipeline Statistics");
        for (const auto& [pipelineStat, stat] : Renderer::GetPipelineStatistics())
            ImGui::Text("%s: %llu", pipelineStat.data(), stat);

        ImGui::SeparatorText("Memory Statistics");
        for (uint32_t memoryHeapIndex = 0; const auto& memoryBudget : rs.MemoryBudgets)
        {
            ImGui::Text("Heap[%u]:\n\tBudget: %0.3f MB\n\tUsage: %0.3f MB\n\tBlocks: %u\n\t(Sub-)DedicatedAllocationsCount: "
                        "%u\n\tDedicatedAllocationsReserved: %0.3f MB\n\tDedicatedAllocationsUsage: %0.3f MB",
                        memoryHeapIndex, memoryBudget.BudgetBytes / 1024.0f / 1024.0f, memoryBudget.UsageBytes / 1024.0f / 1024.0f,
                        memoryBudget.BlockCount, memoryBudget.AllocationCount, memoryBudget.BlockBytes / 1024.0f / 1024.0f,
                        memoryBudget.AllocationBytes / 1024.0f / 1024.0f);
            ++memoryHeapIndex;
        }

        bAnythingHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered();
        bAnythingFocused = ImGui::IsAnyItemFocused() || ImGui::IsWindowFocused();

        ImGui::End();
    }

    static bool bShowWorldOutliner = true;
    if (bShowWorldOutliner) m_WorldOutlinerPanel->OnImGuiRender();

    UILayer::SetBlockEvents(bAnythingHovered || bAnythingFocused);
}

}  // namespace Pathfinder