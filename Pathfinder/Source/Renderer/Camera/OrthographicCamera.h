#ifndef ORTHOGRAPHICCAMERA_H
#define ORTHOGRAPHICCAMERA_H

#include "Camera.h"
#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Input.h"

#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Events/WindowEvent.h"

namespace Pathfinder
{

class OrthographicCamera final : public Camera
{
  public:
    OrthographicCamera() : Camera()
    {
        const auto& window = Application::Get().GetWindow();
        m_AR               = static_cast<float>(window->GetSpecification().Width) / static_cast<float>(window->GetSpecification().Height);

        m_Projection = glm::ortho(-s_CameraWidth * m_AR, s_CameraWidth * m_AR, -s_CameraWidth, s_CameraWidth,
                                  /*0.0001f, 1.0f*/ -s_CameraWidth, s_CameraWidth);

        m_ZoomLevel = s_CameraWidth;
        RecalculateViewMatrix();
    }

    ~OrthographicCamera() override = default;

  protected:
    static constexpr float s_CameraWidth = 8.0f;
    float m_AR                           = 1.0f;

    void OnUpdate(const float deltaTime) final override
    {
        m_DeltaTime = deltaTime;

        if (Input::IsKeyPressed(EKey::KEY_S)) m_Position -= m_Up * s_MovementSpeed * deltaTime;
        if (Input::IsKeyPressed(EKey::KEY_W)) m_Position += m_Up * s_MovementSpeed * deltaTime;

        if (Input::IsKeyPressed(EKey::KEY_A)) m_Position -= m_Right * s_MovementSpeed * deltaTime;
        if (Input::IsKeyPressed(EKey::KEY_D)) m_Position += m_Right * s_MovementSpeed * deltaTime;

        if (Input::IsKeyPressed(EKey::KEY_Q)) m_Rotation.z -= s_MovementSpeed * deltaTime * 2;
        if (Input::IsKeyPressed(EKey::KEY_E)) m_Rotation.z += s_MovementSpeed * deltaTime * 2;

        RecalculateViewMatrix();
    }

    void OnEvent(Event& e) final override
    {
        if (e.GetType() == EEventType::EVENT_TYPE_MOUSE_SCROLLED)
        {
            PFR_ASSERT(m_ZoomSpeed > 0, "Zoom speed can't be negative!");
            if (m_ZoomLevel < m_ZoomSpeed * 2) m_ZoomLevel = m_ZoomSpeed * 2;

            const auto& mouseMovedEvent = (MouseScrolledEvent&)e;
            m_ZoomLevel -= mouseMovedEvent.GetOffsetY() * m_ZoomSpeed;

            m_Projection = glm::ortho(-m_ZoomLevel * m_AR, m_ZoomLevel * m_AR, -m_ZoomLevel, m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
        }

        if (e.GetType() == EEventType::EVENT_TYPE_WINDOW_RESIZE)
        {
            const auto& window = Application::Get().GetWindow();
            m_AR = static_cast<float>(window->GetSpecification().Width) / static_cast<float>(window->GetSpecification().Height);

            m_Projection = glm::ortho(-m_ZoomLevel * m_AR, m_ZoomLevel * m_AR, -m_ZoomLevel, m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
        }
    }

    void RecalculateViewMatrix() final override
    {
        const glm::mat4 transform =
            glm::translate(glm::mat4(1.0f), m_Position) * glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.z), glm::vec3(0, 0, 1));
        m_View = transform;
    }
};

}  // namespace Pathfinder

#endif  // ORTHOGRAPHICCAMERA_H
