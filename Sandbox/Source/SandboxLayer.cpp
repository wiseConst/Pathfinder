#include "SandboxLayer.h"

namespace Pathfinder
{

#define POINT_LIGHT_TEST 1
#define SPOT_LIGHT_TEST 0

constexpr glm::vec3 minLightPos{-15, -5, -5};
constexpr glm::vec3 maxLightPos{15, 20, 5};

void SandboxLayer::Init()
{
    m_Camera = Camera::Create(ECameraType::CAMERA_TYPE_PERSPECTIVE);

    m_Dummy  = Mesh::Create("Assets/Meshes/dragon/scene.gltf");
    m_Sponza = Mesh::Create("Assets/Meshes/sponza/scene.gltf");
    m_Helmet = Mesh::Create("Assets/Meshes/damaged_helmet/DamagedHelmet.gltf");
    m_Gun    = Mesh::Create("Assets/Meshes/cerberus/scene.gltf");

#if POINT_LIGHT_TEST
    const float radius = 2.5f;
    for (uint32_t i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
        m_PointLights[i].bCastShadows = false;
        m_PointLights[i].Position     = glm::linearRand(minLightPos, maxLightPos);
        m_PointLights[i].Intensity    = glm::linearRand(1.0f, 3.0f);
        m_PointLights[i].Radius       = radius;
        m_PointLights[i].MinRadius    = radius * glm::linearRand(0.15f, 0.80f);

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

    m_PointShadowCaster              = {};
    m_PointShadowCaster.bCastShadows = true;
    m_PointShadowCaster.Color        = glm::vec3(1, 0.1f, 0.2f);
    m_PointShadowCaster.Intensity    = 2.9f;
    m_PointShadowCaster.Radius       = 5.0f;
    m_PointShadowCaster.Position     = glm::vec3(0.0f, 4.0f, 0);
}

void SandboxLayer::Destroy() {}

void SandboxLayer::OnEvent(Event& e)
{
    m_Camera->OnEvent(e);
}

void SandboxLayer::OnUpdate(const float deltaTime)
{
    m_Camera->OnUpdate(deltaTime);

    Renderer::BeginScene(*m_Camera);

    Renderer::SubmitMesh(m_Dummy, glm::translate(glm::mat4(1.f), glm::vec3(5, 1.2, 0)));

    Renderer::SubmitMesh(m_Helmet, glm::translate(glm::mat4(1.f), glm::vec3(0, 5, 0)));

    Renderer::SubmitMesh(m_Sponza);
    Renderer::SubmitMesh(
        m_Gun, glm::translate(glm::mat4(1.f), glm::vec3(-3, 10, 0)) * glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(0, 1, 0)) *
                   glm::rotate(glm::mat4(1.f), glm::radians(-90.f), glm::vec3(1, 0, 0)) * glm::scale(glm::mat4(1.f), glm::vec3(0.03f)));

#if 0
    {
        static double iTime = 0.0;
        iTime += (double)deltaTime;
        DirectionalLight dl = {};
        dl.Color            = glm::vec3(.9f, .6f, .08f);
        dl.Direction        = glm::vec3(-5.f, 2.f, 0.f);
        dl.Intensity        = 5.5f;
        dl.bCastShadows     = true;

        dl.Direction = glm::vec3(.5, .4 * (1. + glm::sin(.5 * iTime)), -1.);

        Renderer::AddDirectionalLight(dl);
    }

    m_PointShadowCaster.Position += glm::vec3(3, 0, 0) * deltaTime;
    if (m_PointShadowCaster.Position.x > maxLightPos.x)
    {
        m_PointShadowCaster.Position.x -= (maxLightPos.x - minLightPos.x);
    }
    Renderer::AddPointLight(m_PointShadowCaster);
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
    static bool bShowDemoWindow = true;
    if (bShowDemoWindow) ImGui::ShowDemoWindow(&bShowDemoWindow);

    static bool bShowRendererSettings = true;
    if (bShowRendererSettings)
    {
        ImGui::Begin("Renderer Settings", &bShowRendererSettings);

        ImGui::Checkbox("VSync", &Renderer::GetRendererSettings().bVSync);
        ImGui::Text("IsWindowHovered: %d", ImGui::IsWindowHovered());

        ImGui::End();
    }

    static bool bShowRenderTargetList = true;
    if (bShowRenderTargetList)
    {
        ImGui::Begin("Renderer Target List", &bShowRenderTargetList);

        for (const auto [name, image] : Renderer::GetRenderTargetList())
        {
            ImGui::Text("%s", name.data());
            UILayer::DrawImage(image, {image->GetSpecification().Width / 2, image->GetSpecification().Height / 2}, {0, 0}, {1, 1});
            ImGui::Separator();
        }

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

        ImGui::End();
    }
}

}  // namespace Pathfinder