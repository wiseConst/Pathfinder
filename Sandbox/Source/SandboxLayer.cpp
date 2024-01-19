#include "SandboxLayer.h"

namespace Pathfinder
{

void SandboxLayer::Init()
{
    m_Kitten = Mesh::Create("Assets/Meshes/kitten/scene.gltf");
    m_Camera = Camera::Create(ECameraType::CAMERA_TYPE_ORTHOGRAPHIC);
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

    Renderer::SubmitMesh(m_Kitten);

    /*constexpr float limit = 5.0f;
    for (float y = -limit; y < limit; y += limit / 10.0f)
    {
        for (float x = -limit; x < limit; x += limit / 10.0f)
        {
            const glm::mat4 transform =
                glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.45f, 0.45f, 0.0));
            Renderer2D::DrawQuad(transform, glm::vec4((x + limit) / (limit * 2), 0.4f, (y + limit) / (limit * 2), 1.0f));
        }
    }*/

    Renderer::EndScene();

    if (Input::IsKeyPressed(EKey::KEY_ESCAPE) && Input::IsKeyPressed(EKey::KEY_LEFT_SHIFT))
    {
        Application::Close();
    }
}

void SandboxLayer::OnUIRender() {}

}  // namespace Pathfinder