#include "SandboxLayer.h"

namespace Pathfinder
{

const glm::vec3 minLightPos{-15, -5, -5};
const glm::vec3 maxLightPos{15, 20, 5};

void SandboxLayer::Init()
{
    m_Camera = Camera::Create(ECameraType::CAMERA_TYPE_PERSPECTIVE);

    // m_Dummy  = Mesh::Create("Assets/Meshes/possum_springs/scene.gltf");
    m_Sponza = Mesh::Create("Assets/Meshes/sponza/scene.gltf");
    m_Helmet = Mesh::Create("Assets/Meshes/sci-fi_helmet/scene.gltf");
    m_Gun    = Mesh::Create("Assets/Meshes/flintlock_pistol/scene.gltf");

    const float radius = 2.f;
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

    // Renderer::SubmitMesh(m_Dummy, glm::rotate(glm::mat4(1.f), glm::radians(-90.f), glm::vec3(1, 0, 0)));
    Renderer::SubmitMesh(m_Helmet, glm::translate(glm::mat4(1.f), glm::vec3(0, 5, 0)));

    Renderer::SubmitMesh(m_Sponza);

    static auto angle           = 0.f;
    constexpr const auto radius = 5.f;
    angle += .05f;

    /* m_GunPos =
        glm::vec3((m_GunPos.x + radius * glm::cos(angle)) * deltaTime, m_GunPos.y, (m_GunPos.z + radius * glm::sin(angle)) * deltaTime);

    Renderer::SubmitMesh(m_Gun, glm::translate(glm::mat4(1.f), m_GunPos) * glm::scale(glm::mat4(1.f), glm::vec3(0.2f)));*/

    {
        DirectionalLight dl = {};
        dl.Color            = glm::vec3(.5f, .5f, .9f);
        dl.Direction        = glm::vec3(2, -5.f, -3.f);
        dl.Intensity        = 1.3f;
        dl.bCastShadows     = true;
        Renderer::AddDirectionalLight(dl);
    }

    /*  {
          DirectionalLight dl = {};
          dl.Color            = glm::vec3(.9f, .9f, .1f);
          dl.Direction        = glm::vec3(-.00001f, -0.05f, 8.f);
          dl.Intensity        = 2.5f;
          dl.bCastShadows     = true;
          Renderer::AddDirectionalLight(dl);
      }*/

    /* for (size_t i = 0; i < m_PointLights.size(); ++i)
     {
         m_PointLights[i].Position += glm::vec3(0, 3.0f, 0) * deltaTime;
         if (m_PointLights[i].Position.y > maxLightPos.y)
         {
             m_PointLights[i].Position.y -= (maxLightPos.y - minLightPos.y);
         }

         Renderer::AddPointLight(m_PointLights[i]);
     }*/

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