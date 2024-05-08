#include "SandboxLayer.h"

namespace Pathfinder
{
static const std::string sceneFilePath = "Scenes/SponzaSonne";

void SandboxLayer::Init()
{
    Application::Get().GetWindow()->SetIconImage("Other/Logo.png");

    m_Camera = Camera::Create(ECameraType::CAMERA_TYPE_PERSPECTIVE);

    m_ActiveScene = MakeShared<Scene>(s_DEFAULT_STRING);
    SceneManager::Deserialize(m_ActiveScene, sceneFilePath);

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

    m_ActiveScene->ForEach<PointLightComponent>(
        [&](const auto entityID, PointLightComponent& plc)
        {
            plc.pl.Position += glm::vec3(0, 3.0f, 0) * deltaTime;
            if (plc.pl.Position.y > maxLightPos.y)
            {
                plc.pl.Position.y -= (maxLightPos.y - minLightPos.y);
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

        std::string currentBlurTypeStr = s_DEFAULT_STRING;
        switch (rs.BlurType)
        {
            case EBlurType::BLUR_TYPE_GAUSSIAN: currentBlurTypeStr = "GAUSSIAN"; break;
            case EBlurType::BLUR_TYPE_MEDIAN: currentBlurTypeStr = "MEDIAN"; break;
            case EBlurType::BLUR_TYPE_BOX: currentBlurTypeStr = "BOX"; break;
        }
        const char* blurItems[3] = {"GAUSSIAN", "MEDIAN", "BOX"};
        ImGui::Text("BlurType");
        if (ImGui::BeginCombo("##blurTypeCombo", currentBlurTypeStr.data()))
        {
            for (uint32_t n{}; n < IM_ARRAYSIZE(blurItems); ++n)
            {
                const auto bIsSeleceted = strcmp(currentBlurTypeStr.data(), blurItems[n]) == 0;
                if (ImGui::Selectable(blurItems[n], bIsSeleceted))
                {
                    currentBlurTypeStr = blurItems[n];
                }
                if (bIsSeleceted) ImGui::SetItemDefaultFocus();
            }

            if (strcmp(currentBlurTypeStr.data(), "GAUSSIAN") == 0)
            {
                rs.BlurType = EBlurType::BLUR_TYPE_GAUSSIAN;
            }
            else if (strcmp(currentBlurTypeStr.data(), "MEDIAN") == 0)
            {
                rs.BlurType = EBlurType::BLUR_TYPE_MEDIAN;
            }
            else if (strcmp(currentBlurTypeStr.data(), "BOX") == 0)
            {
                rs.BlurType = EBlurType::BLUR_TYPE_BOX;
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
                image->GetSpecification().Width > 10 ? image->GetSpecification().Width / 2 : image->GetSpecification().Width * 100;
            const uint32_t imageHeight =
                image->GetSpecification().Height > 10 ? image->GetSpecification().Height / 2 : image->GetSpecification().Height * 100;
            UILayer::DrawImage(image, {imageWidth, imageHeight}, {0, 0}, {1, 1});
            ImGui::Separator();
        }

        bAnythingHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered();
        bAnythingFocused = ImGui::IsAnyItemFocused() || ImGui::IsWindowFocused();

        ImGui::End();
    }

    static bool bShowRendererStats = true;
    if (bShowRendererStats)
    {
        ImGui::Begin("Renderer Statistics", &bShowRendererStats);

        ImGui::SeparatorText("Pass Statistics");
        for (const auto& [pass, passTime] : Renderer::GetPassStatistics())
            ImGui::Text("%s: %0.4f(ms)", pass.data(), passTime);

        ImGui::SeparatorText("Pipeline Statistics");
        for (const auto& [pipelineStat, stat] : Renderer::GetPipelineStatistics())
            ImGui::Text("%s: %llu", pipelineStat.data(), stat);

        const auto& rs = Renderer::GetStats();
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
    if (bShowWorldOutliner)
    {
        const std::string worldOutlinerString = "World Outliner: " + std::to_string(m_ActiveScene->GetEntityCount()) + " entities.";
        ImGui::Begin(worldOutlinerString.c_str(), &bShowWorldOutliner);

        m_ActiveScene->ForEach<TagComponent>([&](const auto entityID, TagComponent& tagComponent)
                                             { ImGui::Text("%s", tagComponent.Tag.c_str()); });

        if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty Entity"))
            {
                m_ActiveScene->CreateEntity();
            }

            ImGui::EndPopup();
        }

        bAnythingHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered();
        bAnythingFocused = ImGui::IsAnyItemFocused() || ImGui::IsWindowFocused();

        ImGui::End();
    }

    UILayer::SetBlockEvents(bAnythingHovered || bAnythingFocused);
}

}  // namespace Pathfinder