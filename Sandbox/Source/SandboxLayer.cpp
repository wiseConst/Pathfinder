#include "SandboxLayer.h"

namespace Pathfinder
{

void SandboxLayer::Init()
{
    m_Kitten = Mesh::Create("Assets/Meshes/kitten/scene.gltf");
    m_Camera = Camera::Create(ECameraType::CAMERA_TPYE_ORTHOGRAPHIC);
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
}

void SandboxLayer::OnUpdate(const float deltaTime)
{
    Renderer::BeginScene(*m_Camera);



    Renderer::EndScene();
}

void SandboxLayer::OnUIRender() {}

}  // namespace Pathfinder