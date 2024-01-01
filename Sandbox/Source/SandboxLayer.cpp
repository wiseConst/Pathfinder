#include "SandboxLayer.h"

namespace Pathfinder
{

void SandboxLayer::Init() {}

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

void SandboxLayer::OnUpdate(const float deltaTime) {}

void SandboxLayer::OnUIRender() {}

}  // namespace Pathfinder