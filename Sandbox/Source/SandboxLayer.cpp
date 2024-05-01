#include "SandboxLayer.h"

namespace Pathfinder
{

#define POINT_LIGHT_TEST 1
#define SPOT_LIGHT_TEST 0

constexpr glm::vec3 minLightPos{-15, -5, -5};
constexpr glm::vec3 maxLightPos{15, 20, 5};

void SandboxLayer::Init()
{
    Application::Get().GetWindow()->SetIconImage("Other/Logo.png");

    m_Camera = Camera::Create(ECameraType::CAMERA_TYPE_PERSPECTIVE);

    m_ActiveScene = MakeShared<Scene>("TestBed");
    m_ActiveScene->CreateEntity();

    m_Dummy  = Mesh::Create("stanford/dragon/scene.gltf");
    m_Sponza = Mesh::Create("sponza/scene.gltf");
    m_Helmet = Mesh::Create("damaged_helmet/DamagedHelmet.gltf");
    m_Gun    = Mesh::Create("flintlock_pistol/scene.gltf");

#if POINT_LIGHT_TEST
    const float radius = 2.5f;
    for (uint32_t i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
        m_PointLights[i].bCastShadows = false;
        m_PointLights[i].Position     = glm::linearRand(minLightPos, maxLightPos);
        m_PointLights[i].Intensity    = glm::linearRand(1.0f, 3.0f);
        m_PointLights[i].Radius       = radius;
        m_PointLights[i].MinRadius    = radius * glm::linearRand(0.15f, 0.95f);

        glm::vec3 color = glm::vec3(0.f);
        do
        {
            color = {glm::linearRand(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1))};
        } while (color.length() < 0.8f);
        m_PointLights[i].Color = color;
    }
#endif

#if SPOT_LIGHT_TEST
    const float radius = 5.f;
    for (uint32_t i = 0; i < MAX_SPOT_LIGHTS; ++i)
    {
        m_SpotLights[i].Height    = glm::linearRand(1.0f, 10.0f);
        m_SpotLights[i].Direction = glm::linearRand(glm::vec3(-1.f), glm::vec3(1.f));

        m_SpotLights[i].Position    = glm::linearRand(minLightPos, maxLightPos);
        m_SpotLights[i].Intensity   = 2.9f;
        m_SpotLights[i].InnerCutOff = glm::cos(glm::radians(radius));
        m_SpotLights[i].OuterCutOff = glm::cos(glm::radians(radius * 0.5));
        glm::vec3 color             = glm::vec3(0.f);
        do
        {
            color = {glm::linearRand(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1))};
        } while (color.length() < 0.8f);
        m_SpotLights[i].Color = color;
    }
#endif

    UILayer::SetDefaultFont("Fonts/Manrope/static/Manrope-Bold.ttf", 18.0f);
}

void SandboxLayer::Destroy() {}

void SandboxLayer::OnEvent(Event& e)
{
    m_Camera->OnEvent(e);

    static bool bF3WasPressed = false;

    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyButtonReleasedEvent>(
        [&](const KeyButtonReleasedEvent& event) -> bool
        {
            if (event.GetModKey() != EModKey::MOD_KEY_ALT) return true;
            if (bF3WasPressed && Input::IsKeyReleased(EKey::KEY_F3)) bRenderUI = !bRenderUI;

            return true;
        });

    bF3WasPressed = Input::IsKeyPressed(EKey::KEY_F3);
}

void SandboxLayer::OnUpdate(const float deltaTime)
{
    m_Camera->OnUpdate(deltaTime);

    Renderer::BeginScene(*m_Camera);

    m_ActiveScene->OnUpdate(deltaTime);

    {
        const glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(5, 1.2, 0));
        Renderer::SubmitMesh(m_Dummy, transform);
        DebugRenderer::DrawAABB(m_Dummy, transform, glm::vec4(1, 1, 0, 1));
    }

    {
        const glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(0, 5, 0));
        Renderer::SubmitMesh(m_Helmet, transform);
        DebugRenderer::DrawAABB(m_Helmet, transform, glm::vec4(1, 1, 0, 1));
    }

    {
        Renderer::SubmitMesh(m_Sponza);
        //   DebugRenderer::DrawAABB(m_Sponza, glm::mat4(1.f), {0, 1, 1, 1});
    }

    {
        const glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(-3, 10, 0)) *
                                    glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(0, 1, 0)) /* *
glm::rotate(glm::mat4(1.f), glm::radians(-90.f), glm::vec3(1, 0, 0))*/
                                    * glm::scale(glm::mat4(1.f), glm::vec3(0.09f));
        Renderer::SubmitMesh(m_Gun, transform);
        //  DebugRenderer::DrawAABB(m_Gun, transform, glm::vec4(1, 0, 1, 1));
        DebugRenderer::DrawSphere(m_Gun, transform, glm::vec4(1, 0, 1, 1));
    }

    // DebugRenderer::DrawLine(glm::vec3(0.f), glm::vec3(5.f), glm::vec4(1, 0, 1, 1));

#if 0
    {
        static double iTime = 0.0;
        iTime += (double)deltaTime;
        DirectionalLight dl = {};
        dl.Color            = glm::vec3(.9f, .65f, .1f);
        dl.Direction        = glm::vec3(-5.f, 2.f, 0.f);
        dl.Intensity        = 1.5f;
        //      dl.bCastShadows     = true;

        //    dl.Direction = glm::vec3(.5, .4 * (1. + glm::sin(.5 * iTime)), -1.);

        Renderer::AddDirectionalLight(dl);
    }

#endif

#if SPOT_LIGHT_TEST
    for (auto& spl : m_SpotLights)
    {
        spl.Position += glm::vec3(0, 3.0f, 0) * deltaTime;
        if (spl.Position.y > maxLightPos.y)
        {
            spl.Position.y -= (maxLightPos.y - minLightPos.y);
        }

        Renderer::AddSpotLight(spl);
    }
#endif

#if POINT_LIGHT_TEST
    for (auto& pl : m_PointLights)
    {
        pl.Position += glm::vec3(0, 3.0f, 0) * deltaTime;
        if (pl.Position.y > maxLightPos.y)
        {
            pl.Position.y -= (maxLightPos.y - minLightPos.y);
        }

        Renderer::AddPointLight(pl);
    }
#endif

#if fonarik
    const SpotLight sl = {m_Camera->GetPosition(),     m_Camera->GetForwardVector(), glm::vec3(.4f, .4f, .2f), 2.5f, 100.0f, 5.5f,
                          glm::cos(glm::radians(7.f)), glm::cos(glm::radians(20.f))};
    Renderer::AddSpotLight(sl);
#endif

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
            UILayer::DrawImage(image, {image->GetSpecification().Width / 2, image->GetSpecification().Height / 2}, {0, 0}, {1, 1});
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
        ImGui::Begin("World Outliner", &bShowWorldOutliner);

        if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty Entity"))
            {
                /* m_Context->CreateEntity("Empty Entity");*/
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