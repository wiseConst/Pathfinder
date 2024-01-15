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
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyButtonPressedEvent>(
        [this](auto& event)
        {
            const auto key = event.GetKey();

            if (key == EKey::KEY_ESCAPE)
            {
                Application::Close();
                return true;
            }

            return false;
        });

    m_Camera->OnEvent(e);
}

void SandboxLayer::OnUpdate(const float deltaTime)
{
    m_Camera->OnUpdate(deltaTime);

    Renderer::BeginScene(*m_Camera);

    constexpr int32_t limit = 49;
    for (int32_t x = 0; x < limit; ++x)
    {
        for (int32_t y = 0; y < limit; ++y)
        {
            const glm::mat4 transform =
                glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.45f, 0.45f, 0.0));
            Renderer2D::DrawQuad(transform, glm::vec4(0.9f, 0.3f, 0.2f, 1.0f));
        }
    }

    Renderer::EndScene();
}

void SandboxLayer::OnUIRender() {}

}  // namespace Pathfinder