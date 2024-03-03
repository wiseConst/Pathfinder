#include "SandboxLayer.h"

namespace Pathfinder
{

const glm::vec3 minLightPos{-15, -5, -5};
const glm::vec3 maxLightPos{15, 20, 5};

void SandboxLayer::Init()
{
    m_Camera = Camera::Create(ECameraType::CAMERA_TYPE_PERSPECTIVE);

    m_Dummy  = Mesh::Create("Assets/Meshes/dragon/scene.gltf");
    m_Sponza = Mesh::Create("Assets/Meshes/sponza/scene.gltf");
    m_Helmet = Mesh::Create("Assets/Meshes/damaged_helmet/DamagedHelmet.gltf");
    m_Gun    = Mesh::Create("Assets/Meshes/cerberus/scene.gltf");

    const float radius = 2.5f;
    for (uint32_t i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
        m_PointLights[i].Position  = glm::linearRand(minLightPos, maxLightPos);
        m_PointLights[i].Intensity = 1.f;
        m_PointLights[i].Radius    = radius;
        m_PointLights[i].MinRadius = radius * 0.25f;
        glm::vec3 color            = glm::vec3(0.f);
        do
        {
            color = {glm::linearRand(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1))};
        } while (color.length() < 0.8f);
        m_PointLights[i].Color = color;
    }
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

    Renderer::SubmitMesh(m_Dummy, glm::translate(glm::mat4(1.f), glm::vec3(-5, 2, 0)) *
                                      glm::rotate(glm::mat4(1.f), glm::radians(-180.f), glm::vec3(1, 0, 0)));

    Renderer::SubmitMesh(m_Helmet, glm::translate(glm::mat4(1.f), glm::vec3(0, 5, 0)) *
                                       glm::rotate(glm::mat4(1.f), glm::radians(-180.f), glm::vec3(1, 0, 0)));

    Renderer::SubmitMesh(m_Sponza);
    Renderer::SubmitMesh(
        m_Gun, glm::translate(glm::mat4(1.f), glm::vec3(-3, 10, 0)) * glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(0, 1, 0)) *
                   glm::rotate(glm::mat4(1.f), glm::radians(-90.f), glm::vec3(1, 0, 0)) * glm::scale(glm::mat4(1.f), glm::vec3(0.03f)));

    {
        DirectionalLight dl = {};
        dl.Color            = glm::vec3(.4f, .4f, .9f);
        dl.Direction        = glm::vec3(5.f, -2.f, 0.f);
        dl.Intensity        = 2.5f;
        dl.bCastShadows     = true;
        Renderer::AddDirectionalLight(dl);
    }

    /*{
        DirectionalLight dl = {};
        dl.Color            = glm::vec3(.9f, .9f, .2f);
        dl.Direction        = glm::vec3(-5.f, -5.f, 0.f);
        dl.Intensity        = 1.9f;
        // dl.bCastShadows     = true;
        Renderer::AddDirectionalLight(dl);
    }*/

    for (auto& pl : m_PointLights)
    {
        pl.Position += glm::vec3(0, 3.0f, 0) * deltaTime;
        if (pl.Position.y > maxLightPos.y)
        {
            pl.Position.y -= (maxLightPos.y - minLightPos.y);
        }

        Renderer::AddPointLight(pl);
    }

    /*const SpotLight sl = {m_Camera->GetPosition(),      m_Camera->GetForwardVector(), glm::vec3(.4f, .4f, .2f), 1.f,
                          glm::cos(glm::radians(7.f)), glm::cos(glm::radians(20.f))};
    Renderer::AddSpotLight(sl);*/

    Renderer::EndScene();

    if (Input::IsKeyPressed(EKey::KEY_ESCAPE) && Input::IsKeyPressed(EKey::KEY_LEFT_SHIFT))
    {
        Application::Close();
    }
}

void SandboxLayer::OnUIRender() {}

}  // namespace Pathfinder