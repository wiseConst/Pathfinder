#include "SandboxLayer.h"

namespace Pathfinder
{

void SandboxLayer::Init()
{
    m_Dummy  = Mesh::Create("Assets/Meshes/sponza/scene.gltf");
    m_Camera = Camera::Create(ECameraType::CAMERA_TYPE_PERSPECTIVE);
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

    Renderer::SubmitMesh(m_Dummy /*, glm::rotate(glm::mat4(1.0f), glm::radians(-90.f), glm::vec3(1, 0, 0)) *
                                       glm::rotate(glm::mat4(1.0f), glm::radians(-90.f), glm::vec3(0, 0, 1))*/
                         /* ,glm::rotate(glm::mat4(1.f),glm::radians(-180.f), glm::vec3(0, 0, 1))*/);

    /*   DirectionalLight dl = {};
          dl.Color            = glm::vec3(.5f, .5f, .9f);
          dl.Direction        = glm::vec3(0, -3.f, -5.f);
          dl.Intensity        = 1.f;
          Renderer::AddDirectionalLight(dl);*/

    {
        // blueish
        PointLight pl = {};
        pl.Color      = glm::vec3(.5f, .5f, .9f);
        pl.Position   = glm::vec3(0, 2, 0.f);
        pl.Intensity  = 1.5f;
        pl.MinRadius  = .1f;
        pl.Radius     = 3.3f;

        Renderer::AddPointLight(pl);
    }

    {
        // greenish
        PointLight pl = {};
        pl.Color      = glm::vec3(.5f, .9f, 0.5f);
        pl.Position   = glm::vec3(0, 5, -15.f);
        pl.Intensity  = 1.5f;
        pl.MinRadius  = 3.3f;
        pl.Radius     = 7.3f;

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